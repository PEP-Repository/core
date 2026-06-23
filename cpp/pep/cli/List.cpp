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

#include <pep/structuredoutput/Tree.hpp>
#include <pep/structuredoutput/Json.hpp>
#include <pep/structuredoutput/Yaml.hpp>

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
      + pep::commandline::Parameter("show-dataless", "Also output (pseudonyms for) subjects without data")
      + pep::commandline::Parameter("metadata", "Print metadata - which may contain encrypted entries when only an ID was returned for the file in question; apply pepcli get to the ID to get the decrypted entries").shorthand('m')
      + pep::commandline::Parameter("no-inline-data", "Never retrieve data inline; only return IDs")
      + pep::commandline::Parameter("group-output", "Group the output per participant").shorthand('g')
      + pep::commandline::Parameter("format", "The format of the output.").value(pep::commandline::Value<std::string>().allow(std::vector<std::string>({"yaml", "json"})).defaultsTo("json"));
  }

  int execute() override {
    class SubjectData {
    private:
      pep::PolymorphicPseudonym pp_;
      bool collectMetadata_;
      std::optional<std::string> lp_;
      std::optional<std::string> blp_;
      pt::ptree values_;
      pt::ptree metadata_;
      pt::ptree ids_;

    public:
      SubjectData(const pep::EnumerateAndRetrieveResult& ear, bool collectMetadata, std::shared_ptr<pep::GlobalConfiguration> globalConfig)
        : pp_(ear.localPseudonyms->polymorphic), collectMetadata_(collectMetadata) {
        if (ear.accessGroupPseudonym != nullptr) {
          lp_ = ear.accessGroupPseudonym->text();
          if (globalConfig) {
            blp_ = globalConfig->getUserPseudonymFormat().makeUserPseudonym(*ear.accessGroupPseudonym);
          }
        }
        this->add(ear);
      }

      SubjectData(pep::PolymorphicPseudonym pp, const std::optional<pep::LocalPseudonym> lp, std::shared_ptr<pep::GlobalConfiguration> globalConfig)
        : pp_(pp), collectMetadata_(false), lp_(pep::GetOptionalValue(lp, std::mem_fn(&pep::LocalPseudonym::text))) {
        if (lp.has_value() && globalConfig) {
          blp_ = globalConfig->getUserPseudonymFormat().makeUserPseudonym(*lp);
        }
      }

      const pep::PolymorphicPseudonym& pp() const noexcept { return pp_; }

      bool hasData() const {
        return !values_.empty();
      }

      void add(const pep::EnumerateAndRetrieveResult& ear) {
        assert(pp_ == ear.localPseudonyms->polymorphic);

        if (ear.dataSet) {
          values_.push_back(pt::ptree::value_type(
            ear.column, ear.data));
        }
        else {
          ids_.push_back(pt::ptree::value_type(
            ear.column, boost::algorithm::hex(ear.id)));
        }

        if (collectMetadata_) {
          pt::ptree mdpt;
          pep::Metadata md = ear.metadataDecrypted
            ? *(ear.metadataDecrypted) : ear.metadata;

          // woefully inefficient, but consistency is assured
          auto mdpb = pep::Serialization::ToProtocolBuffer(std::move(md));
          std::string mdjson;
          if (const auto status = google::protobuf::util::MessageToJsonString(mdpb, &mdjson); !status.ok()) {
            throw std::runtime_error("Failed to convert metadata to JSON: " + status.ToString());
          }
          std::istringstream ss(std::move(mdjson));
          pt::json_parser::read_json(ss, mdpt);

          metadata_.push_back(pt::ptree::value_type(
            ear.column, std::move(mdpt)));
        }
      }

      pt::ptree toPropertyTree() const {
        pt::ptree tree;
        if (!values_.empty())
          tree.add_child("data", values_);
        if (!ids_.empty())
          tree.add_child("ids", ids_);
        if (!metadata_.empty())
          tree.add_child("metadata", metadata_);
        tree.put("pp", pp_.text());
        if (lp_.has_value())
          tree.put("lp", *lp_);
        if (blp_.has_value())
          tree.put("blp", *blp_);
        return tree;
      }
    };

    struct Context {
      const pep::commandline::NamedValues parameterValues;
      bool hasPrintedData = false;
      pep::EnumerateAndRetrieveData2Opts earOpts;
      bool printMetadata = false;
      bool groupOutput = false;
      std::string format;
      std::shared_ptr<pep::GlobalConfiguration> globalConfig;
      std::unordered_map<uint32_t, SubjectData> subjects;
      size_t dataCount{ 0 };
      std::unordered_map<pep::PolymorphicPseudonym, std::optional<pep::EncryptedLocalPseudonym>> pseudsToReport;
      pt::ptree results;

      explicit Context(const pep::commandline::NamedValues& parameterValues)
        : parameterValues(parameterValues) {
      }

      void collectSubjects() {
        for (const auto& entry : subjects) {
          results.push_back(pt::ptree::value_type("", entry.second.toPropertyTree()));
          if (entry.second.hasData()) {
            hasPrintedData = true;
          }
          pseudsToReport.erase(entry.second.pp());
        }
        subjects.clear();
      }

      void printRemainingPseudsToReport(std::shared_ptr<pep::CoreClient> client) {
        assert(subjects.empty());

        // For each pseudonym-to-report that we haven't produced output for...
        uint32_t index = 0; // ...use a unique (but meaningless) index...
        for (const auto& report : pseudsToReport) {
          std::optional<pep::LocalPseudonym> decrypted;
          if (report.second.has_value()) {
            decrypted = client->decryptLocalPseudonym(*report.second);
          }
          SubjectData data(report.first, decrypted, globalConfig);
          subjects.emplace(std::make_pair(index++, std::move(data))); // ...to add an entry to our "subjects" field...
        }

        this->collectSubjects(); // ...then produce output for all the "subjects" that we just stored
        assert(pseudsToReport.empty());
      }

      void processResult(const pep::EnumerateAndRetrieveResult& ear) {
        dataCount++;
        auto existing = subjects.find(ear.localPseudonymsIndex);
        if (existing == subjects.cend()) {
          if (!groupOutput) {
            collectSubjects();
          }
          [[maybe_unused]] auto emplaced = subjects.emplace(ear.localPseudonymsIndex, SubjectData(ear, printMetadata, globalConfig));
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

          std::cerr << std::move(out).str();
      }
    };
    auto ctx = std::make_shared<Context>(this->getParameterValues());

    return this->executeEventLoopFor([ctx](std::shared_ptr<pep::CoreClient> client) {
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
        ctx->format = ctx->parameterValues.get<std::string>("format");

        // Only fetch GlobalConfiguration if we need it for brief local pseudonyms
        rxcpp::observable<pep::FakeVoid> configObservable;
        if (ctx->earOpts.includeAccessGroupPseudonyms) {
          configObservable = client->getGlobalConfiguration().map([ctx](std::shared_ptr<pep::GlobalConfiguration> gc) { 
              ctx->globalConfig = gc; 
              return pep::FakeVoid(); 
            });
        } else {
          configObservable = rxcpp::observable<>::just(pep::FakeVoid());
        }

        return configObservable.flat_map([ctx, client](pep::FakeVoid) {

        pep::RequestTicket2Opts tOpts;
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
          if (ctx->parameterValues.has("show-dataless")) {
            auto pseuds = ctx->earOpts.ticket->openTicketWithoutCheckingSignature()->accessSubjects;
            std::transform(pseuds.begin(), pseuds.end(), std::inserter(ctx->pseudsToReport, ctx->pseudsToReport.begin()), [](const pep::LocalPseudonyms& lps) {
              return std::make_pair(lps.polymorphic, lps.accessGroup);
              });
          }
          return client->enumerateAndRetrieveData2(ctx->earOpts);
        });
        });
      }).map([ctx](pep::EnumerateAndRetrieveResult result) {
        ctx->processResult(result);

        return pep::FakeVoid();
      })
      .as_dynamic() // Reduce compiler memory usage
      .op(pep::RxBeforeCompletion(
      [ctx, client]() {
        ctx->collectSubjects();
        ctx->printRemainingPseudsToReport(client);
        
        // Convert ptree to Tree and output with selected format
        auto tree = pep::structuredOutput::Tree::FromPropertyTree(ctx->results);
        if (ctx->format == "json") {
          pep::structuredOutput::json::append(std::cout, tree) << std::endl;
        } else {
          pep::structuredOutput::yaml::append(std::cout, tree) << std::endl;
        }
        
        ctx->printQueryInfo();
        if (ctx->hasPrintedData) {
          PEP_LOG(LogTag, pep::Severity::Warning) << "Data may require re-pseudonymization. Please use `pepcli pull` instead to ensure it is processed properly.";
        }
      }));
      });
  }
};

}

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandList(CliApplication& parent) {
  return std::make_shared<CommandList>(parent);
}
