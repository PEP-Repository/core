#include <pep/application/Application.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/File.hpp>
#include <pep/async/RxUtils.hpp>
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
      + pep::commandline::Parameter("metadata", "Print metadata - which may contain encrypted entries when only an ID was returned for the file in question; apply pepcli get to the ID to get the decrypted entries").shorthand('m')
      + pep::commandline::Parameter("no-inline-data", "Never retrieve data inline; only return IDs")
      + pep::commandline::Parameter("group-output", "Group the output per participant").shorthand('g');
  }

  int execute() override {
    class SubjectData {
    private:
      std::string mPp;
      bool mCollectMetadata;
      std::optional<std::string> mLp;
      pt::ptree mValues;
      pt::ptree mMetadata;
      pt::ptree mIds;

    public:
      SubjectData(const pep::EnumerateAndRetrieveResult& ear, bool collectMetadata)
        : mPp(ear.mLocalPseudonyms->mPolymorphic.text()), mCollectMetadata(collectMetadata) {
        if (ear.mAccessGroupPseudonym != nullptr) {
          mLp = ear.mAccessGroupPseudonym->text();
        }
        this->add(ear);
      }

      bool hasData() const {
        return !mValues.empty();
      }

      void add(const pep::EnumerateAndRetrieveResult& ear) {
        assert(mPp == ear.mLocalPseudonyms->mPolymorphic.text());

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
        toPrint.put("pp", mPp);
        if (mLp.has_value())
          toPrint.put("lp", *mLp);
        pt::write_json(std::cout, toPrint);
      }
    };

    struct Context {
      const pep::commandline::NamedValues parameterValues;
      bool hadPrevious = false;
      bool hasPrintedData = false;
      pep::enumerateAndRetrieveData2Opts earOpts;
      bool printMetadata = false;
      bool groupOutput = false;
      std::unordered_map<uint32_t, SubjectData> subjects;
      size_t dataCount{ 0 };

      explicit Context(const pep::commandline::NamedValues& parameterValues)
        : parameterValues(parameterValues) {
      }

      void printAndClearSubjects() {
        for (const auto& entry : subjects) {
          if (hadPrevious) {
            std::cout << ",";
          }
          else {
            hadPrevious = true;
          }
          entry.second.print(std::cout);
          if (entry.second.hasData()) {
            hasPrintedData = true;
          }
        }
        subjects.clear();
      }

      void processResult(const pep::EnumerateAndRetrieveResult& ear) {
        dataCount++;
        auto existing = subjects.find(ear.mLocalPseudonymsIndex);
        if (existing == subjects.cend()) {
          if (!groupOutput) {
            printAndClearSubjects();
          }
          [[maybe_unused]] auto emplaced = subjects.emplace(ear.mLocalPseudonymsIndex, SubjectData(ear, printMetadata));
          assert(emplaced.second);
        }
        else {
          existing->second.add(ear);
        }
      }

      void printQueryInfo() {
          std::ostringstream out;

          out << "Listed " << dataCount << " results for: ";
          if (earOpts.columnGroups.size() > 0 || earOpts.columns.size() == 0) {
              out << earOpts.columnGroups.size() << " Column Group(s) ";
          }
          if (earOpts.columns.size() > 0) {
              out << earOpts.columns.size() << " Column(s) ";
          }
          out << "and ";
          if (earOpts.pps.size() > 0) {
              out << earOpts.pps.size() << " Participant(s) ";
          }
          if (earOpts.groups.size() > 0 || earOpts.pps.size() == 0) {
              out << earOpts.groups.size() << " Participant Group(s)";
          }
          out << std::endl;

          std::cerr << out.str();
      }
    };
    auto ctx = std::make_shared<Context>(this->getParameterValues());

    return this->executeEventLoopFor([ctx](std::shared_ptr<pep::CoreClient> client) {
      std::cout << '[';

      return MultiCellQuery::GetPps(ctx->parameterValues, client)
        .op(pep::RxToVector())
      .as_dynamic() // Reduce compiler memory usage
      .flat_map([client, ctx](std::shared_ptr<std::vector<pep::PolymorphicPseudonym>> all_pps){
        ctx->earOpts.groups = MultiCellQuery::GetParticipantGroups(ctx->parameterValues);
        ctx->earOpts.pps = *all_pps;
        ctx->earOpts.columnGroups = MultiCellQuery::GetColumnGroups(ctx->parameterValues);
        ctx->earOpts.columns = MultiCellQuery::GetColumns(ctx->parameterValues);

        if (ctx->parameterValues.has("no-inline-data")) {
          ctx->earOpts.includeData = false;
        } else {
          ctx->earOpts.includeData = true;
          ctx->earOpts.dataSizeLimit = ctx->parameterValues.get<uint64_t>("inline-data-size-limit");
        }
        ctx->earOpts.forceTicket = true;
        ctx->earOpts.includeAccessGroupPseudonyms = ctx->parameterValues.has("local-pseudonyms");
        ctx->printMetadata = ctx->parameterValues.has("metadata");
        ctx->groupOutput = ctx->parameterValues.has("group-output");

        pep::requestTicket2Opts tOpts;
        tOpts.pps = ctx->earOpts.pps;
        tOpts.columns = ctx->earOpts.columns;
        tOpts.columnGroups = ctx->earOpts.columnGroups;
        tOpts.participantGroups = ctx->earOpts.groups;
        tOpts.modes = {ctx->earOpts.includeData ? "read" : "read-meta"};
        tOpts.includeAccessGroupPseudonyms = ctx->earOpts.includeAccessGroupPseudonyms;
        return pep::cli::TicketFile::GetTicket(*client, ctx->parameterValues, tOpts)
        .flat_map([client, ctx](pep::IndexedTicket2 ticket)
            -> rxcpp::observable<pep::EnumerateAndRetrieveResult> {
          ctx->earOpts.ticket = std::make_shared<pep::IndexedTicket2>(
              std::move(ticket));
          return client->enumerateAndRetrieveData2(ctx->earOpts);
        });
      }).map([ctx](pep::EnumerateAndRetrieveResult result) {
        ctx->processResult(result);

        return pep::FakeVoid();
      })
      .as_dynamic() // Reduce compiler memory usage
      .op(pep::RxBeforeCompletion(
      [ctx]() {
        ctx->printAndClearSubjects();
        std::cout << ']' << std::endl;
        ctx->printQueryInfo();
        if (ctx->hasPrintedData) {
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
