#include <pep/application/Application.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/File.hpp>
#include <pep/utils/MiscUtil.hpp>
#include <pep/async/RxBeforeCompletion.hpp>
#include <pep/async/RxToVector.hpp>
#include <pep/core-client/CoreClient.hpp>
#include <pep/morphing/MorphingSerializers.hpp>

#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/cli/MultiCellQuery.hpp>
#include <pep/cli/TicketFile.hpp>

#include <iostream>

#include <boost/algorithm/hex.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/lexical_cast.hpp>

#include <google/protobuf/util/json_util.h>

#include <rxcpp/operators/rx-flat_map.hpp>

using namespace pep::cli;
namespace pt = boost::property_tree;

namespace {

class CommandList : public ChildCommandOf<CliApplication> {
public:
  explicit CommandList(CliApplication& parent)
    : ChildCommandOf<CliApplication>("list", "Query, retrieve and print data", parent) {
  }

protected:
  inline std::optional<std::string> getAdditionalDescription() const override {
    return "Retrieve and print specified columns/column groups for specified participants/participant groups.";
  }

  inline std::optional<std::string> getRelativeDocumentationUrl() const override { return "using-pepcli#list"; }

  pep::commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf<CliApplication>::getSupportedParameters()
      + MultiCellQuery::Parameters()
      + pep::cli::TicketFile::GetParameters(true)
      + pep::commandline::Parameter("inline-data-size-limit", "Retrieve data inline if size is less than this. Specify 0 to inline all data.").shorthand('s').value(
          pep::commandline::Value<uint64_t>().defaultsTo(1000))
      + pep::commandline::Parameter("local-pseudonyms", "Request access group local-pseudonyms").shorthand('l')
      + pep::commandline::Parameter("dataless", "Also output (pseudonyms for) subjects without data")
      + pep::commandline::Parameter("metadata", "Print metadata - which may contain encrypted entries when only an ID was returned for the file in question; apply pepcli get to the ID to get the decrypted entries").shorthand('m')
      + pep::commandline::Parameter("no-inline-data", "Never retrieve data inline; only return IDs")
      + pep::commandline::Parameter("group-output", "Group the output per participant").shorthand('g');
  }

  int execute() override {
    class SubjectData {
    private:
      pep::PolymorphicPseudonym mPp;
      bool mCollectMetadata;
      std::optional<std::string> mLp;
      pt::ptree mValues;
      pt::ptree mMetadata;
      pt::ptree mIds;

    public:
      SubjectData(const pep::EnumerateAndRetrieveResult& ear, bool collectMetadata)
        : mPp(ear.mLocalPseudonyms->mPolymorphic), mCollectMetadata(collectMetadata) {
        if (ear.mAccessGroupPseudonym != nullptr) {
          mLp = ear.mAccessGroupPseudonym->text();
        }
        this->add(ear);
      }

      SubjectData(pep::PolymorphicPseudonym pp, const std::optional<pep::LocalPseudonym> lp)
        : mPp(pp), mCollectMetadata(false), mLp(pep::GetOptionalValue(lp, std::mem_fn(&pep::LocalPseudonym::text))) {
      }

      const pep::PolymorphicPseudonym& pp() const noexcept { return mPp; }

      bool hasData() const {
        return !mValues.empty();
      }

      void add(const pep::EnumerateAndRetrieveResult& ear) {
        assert(mPp == ear.mLocalPseudonyms->mPolymorphic);

        if (ear.mDataSet) {
          mValues.push_back(pt::ptree::value_type(
            ear.mColumn, ear.mData));
        }
        else {
          mIds.push_back(pt::ptree::value_type(
            ear.mColumn, boost::algorithm::hex(ear.mId)));
        }

        if (mCollectMetadata) {
          pt::ptree mdpt;
          pep::Metadata md = ear.mMetadataDecrypted
            ? *(ear.mMetadataDecrypted) : ear.mMetadata;

          // woefully inefficient, but consistency is assured
          auto mdpb = pep::Serialization::ToProtocolBuffer(std::move(md));
          std::string mdjson;
          if (const auto status = google::protobuf::util::MessageToJsonString(mdpb, &mdjson); !status.ok()) {
            throw std::runtime_error("Failed to convert metadata to JSON: " + status.ToString());
          }
          std::istringstream ss(std::move(mdjson));
          pt::json_parser::read_json(ss, mdpt);

          mMetadata.push_back(pt::ptree::value_type(
            ear.mColumn, std::move(mdpt)));
        }
      }

      void print(std::ostream& destination) const {
        pt::ptree toPrint;
        if (!mValues.empty())
          toPrint.add_child("data", mValues);
        if (!mIds.empty())
          toPrint.add_child("ids", mIds);
        if (!mMetadata.empty())
          toPrint.add_child("metadata", mMetadata);
        toPrint.put("pp", mPp.text());
        if (mLp.has_value())
          toPrint.put("lp", *mLp);
        pt::write_json(std::cout, toPrint);
      }
    };

    struct Context {
      const pep::commandline::NamedValues mParameterValues;
      bool mHadPrevious = false;
      bool mHasPrintedData = false;
      pep::enumerateAndRetrieveData2Opts mEarOpts;
      bool mPrintMetadata = false;
      bool mGroupOutput = false;
      std::unordered_map<uint32_t, SubjectData> mSubjects;
      size_t mDataCount{ 0 };
      std::unordered_map<pep::PolymorphicPseudonym, std::optional<pep::EncryptedLocalPseudonym>> mPseudsToReport;

      explicit Context(const pep::commandline::NamedValues& parameterValues)
        : mParameterValues(parameterValues) {
      }

      void printAndClearSubjects() {
        for (const auto& entry : mSubjects) {
          if (mHadPrevious) {
            std::cout << ",";
          }
          else {
            mHadPrevious = true;
          }
          entry.second.print(std::cout);
          if (entry.second.hasData()) {
            mHasPrintedData = true;
          }
          mPseudsToReport.erase(entry.second.pp());
        }
        mSubjects.clear();
      }

      void printRemainingPseudsToReport(std::shared_ptr<pep::CoreClient> client) {
        assert(mSubjects.empty());

        // For each pseudonym-to-report that we haven't produced output for...
        uint32_t index = 0; // ...use a unique (but meaningless) index...
        for (const auto& report : mPseudsToReport) {
          std::optional<pep::LocalPseudonym> decrypted;
          if (report.second.has_value()) {
            decrypted = client->decryptLocalPseudonym(*report.second);
          }
          SubjectData data(report.first, decrypted);
          mSubjects.emplace(std::make_pair(index++, std::move(data))); // ...to add an entry to our "subjects" field...
        }

        this->printAndClearSubjects(); // ...then produce output for all the "subjects" that we just stored
        assert(mPseudsToReport.empty());
      }

      void processResult(const pep::EnumerateAndRetrieveResult& ear) {
        mDataCount++;
        auto existing = mSubjects.find(ear.mLocalPseudonymsIndex);
        if (existing == mSubjects.cend()) {
          if (!mGroupOutput) {
            printAndClearSubjects();
          }
          [[maybe_unused]] auto emplaced = mSubjects.emplace(ear.mLocalPseudonymsIndex, SubjectData(ear, mPrintMetadata));
          assert(emplaced.second);
        }
        else {
          existing->second.add(ear);
        }
      }

      void printQueryInfo() {
          std::ostringstream out;

          out << "Listed " << mDataCount << " results for: ";
          if (mEarOpts.columnGroups.size() > 0 || mEarOpts.columns.size() == 0) {
              out << mEarOpts.columnGroups.size() << " Column Group(s) ";
          }
          if (mEarOpts.columns.size() > 0) {
              out << mEarOpts.columns.size() << " Column(s) ";
          }
          out << "and ";
          if (mEarOpts.pps.size() > 0) {
              out << mEarOpts.pps.size() << " Participant(s) ";
          }
          if (mEarOpts.groups.size() > 0 || mEarOpts.pps.size() == 0) {
              out << mEarOpts.groups.size() << " Participant Group(s)";
          }
          out << std::endl;

          std::cerr << std::move(out).str();
      }
    };
    auto ctx = std::make_shared<Context>(this->getParameterValues());

    return this->executeEventLoopFor([ctx](std::shared_ptr<pep::CoreClient> client) {
      std::cout << '[';

      return MultiCellQuery::GetPps(ctx->mParameterValues, client)
        .op(pep::RxToVector())
      .as_dynamic() // Reduce compiler memory usage
      .flat_map([client, ctx](std::shared_ptr<std::vector<pep::PolymorphicPseudonym>> all_pps){
        ctx->mEarOpts.groups = MultiCellQuery::GetParticipantGroups(ctx->mParameterValues);
        ctx->mEarOpts.pps = *all_pps;
        ctx->mEarOpts.columnGroups = MultiCellQuery::GetColumnGroups(ctx->mParameterValues);
        ctx->mEarOpts.columns = MultiCellQuery::GetColumns(ctx->mParameterValues);

        if (ctx->mParameterValues.has("no-inline-data")) {
          ctx->mEarOpts.includeData = false;
        } else {
          ctx->mEarOpts.includeData = true;
          ctx->mEarOpts.dataSizeLimit = ctx->mParameterValues.get<uint64_t>("inline-data-size-limit");
        }
        ctx->mEarOpts.forceTicket = true;
        ctx->mEarOpts.includeAccessGroupPseudonyms = ctx->mParameterValues.has("local-pseudonyms");
        ctx->mPrintMetadata = ctx->mParameterValues.has("metadata");
        ctx->mGroupOutput = ctx->mParameterValues.has("group-output");

        pep::requestTicket2Opts tOpts;
        tOpts.pps = ctx->mEarOpts.pps;
        tOpts.columns = ctx->mEarOpts.columns;
        tOpts.columnGroups = ctx->mEarOpts.columnGroups;
        tOpts.participantGroups = ctx->mEarOpts.groups;
        tOpts.modes = {ctx->mEarOpts.includeData ? "read" : "read-meta"};
        tOpts.includeAccessGroupPseudonyms = ctx->mEarOpts.includeAccessGroupPseudonyms;
        return pep::cli::TicketFile::GetTicket(*client, ctx->mParameterValues, tOpts)
        .flat_map([client, ctx](pep::IndexedTicket2 ticket)
            -> rxcpp::observable<pep::EnumerateAndRetrieveResult> {
          ctx->mEarOpts.ticket = std::make_shared<pep::IndexedTicket2>(
              std::move(ticket));
          if (ctx->mParameterValues.has("dataless")) {
            auto pseuds = ctx->mEarOpts.ticket->openTicketWithoutCheckingSignature()->mPseudonyms;
            std::transform(pseuds.begin(), pseuds.end(), std::inserter(ctx->mPseudsToReport, ctx->mPseudsToReport.begin()), [](const pep::LocalPseudonyms& lps) {
              return std::make_pair(lps.mPolymorphic, lps.mAccessGroup);
              });
          }
          return client->enumerateAndRetrieveData2(ctx->mEarOpts);
        });
      }).map([ctx](pep::EnumerateAndRetrieveResult result) {
        ctx->processResult(result);

        return pep::FakeVoid();
      })
      .as_dynamic() // Reduce compiler memory usage
      .op(pep::RxBeforeCompletion(
      [ctx, client]() {
        ctx->printAndClearSubjects();
        ctx->printRemainingPseudsToReport(client);
        std::cout << ']' << std::endl;
        ctx->printQueryInfo();
        if (ctx->mHasPrintedData) {
          LOG(LOG_TAG, pep::warning) << "Data may require re-pseudonymization. Please use `pepcli pull` instead to ensure it is processed properly.";
        }
      }));
      });
  }
};

}

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandList(CliApplication& parent) {
  return std::make_shared<CommandList>(parent);
}
