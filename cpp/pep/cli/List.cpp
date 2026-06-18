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
        : pp_(ear.localPseudonyms_->polymorphic_), collectMetadata_(collectMetadata) {
        if (ear.accessGroupPseudonym_ != nullptr) {
          lp_ = ear.accessGroupPseudonym_->text();
          if (globalConfig) {
            blp_ = globalConfig->getUserPseudonymFormat().makeUserPseudonym(*ear.accessGroupPseudonym_);
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
        assert(pp_ == ear.localPseudonyms_->polymorphic_);

        if (ear.dataSet_) {
          values_.push_back(pt::ptree::value_type(
            ear.column_, ear.data_));
        }
        else {
          ids_.push_back(pt::ptree::value_type(
            ear.column_, boost::algorithm::hex(ear.id_)));
        }

        if (collectMetadata_) {
          pt::ptree mdpt;
          pep::Metadata md = ear.metadataDecrypted_
            ? *(ear.metadataDecrypted_) : ear.metadata_;

          // woefully inefficient, but consistency is assured
          auto mdpb = pep::Serialization::ToProtocolBuffer(std::move(md));
          std::string mdjson;
          if (const auto status = google::protobuf::util::MessageToJsonString(mdpb, &mdjson); !status.ok()) {
            throw std::runtime_error("Failed to convert metadata to JSON: " + status.ToString());
          }
          std::istringstream ss(std::move(mdjson));
          pt::json_parser::read_json(ss, mdpt);

          metadata_.push_back(pt::ptree::value_type(
            ear.column_, std::move(mdpt)));
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
      const pep::commandline::NamedValues parameterValues_;
      bool hasPrintedData_ = false;
      pep::EnumerateAndRetrieveData2Opts earOpts_;
      bool printMetadata_ = false;
      bool groupOutput_ = false;
      std::string format_;
      std::shared_ptr<pep::GlobalConfiguration> globalConfig_;
      std::unordered_map<uint32_t, SubjectData> subjects_;
      size_t dataCount_{ 0 };
      std::unordered_map<pep::PolymorphicPseudonym, std::optional<pep::EncryptedLocalPseudonym>> pseudsToReport_;
      pt::ptree results_;

      explicit Context(const pep::commandline::NamedValues& parameterValues)
        : parameterValues_(parameterValues) {
      }

      void collectSubjects() {
        for (const auto& entry : subjects_) {
          results_.push_back(pt::ptree::value_type("", entry.second.toPropertyTree()));
          if (entry.second.hasData()) {
            hasPrintedData_ = true;
          }
          pseudsToReport_.erase(entry.second.pp());
        }
        subjects_.clear();
      }

      void printRemainingPseudsToReport(std::shared_ptr<pep::CoreClient> client) {
        assert(subjects_.empty());

        // For each pseudonym-to-report that we haven't produced output for...
        uint32_t index = 0; // ...use a unique (but meaningless) index...
        for (const auto& report : pseudsToReport_) {
          std::optional<pep::LocalPseudonym> decrypted;
          if (report.second.has_value()) {
            decrypted = client->decryptLocalPseudonym(*report.second);
          }
          SubjectData data(report.first, decrypted, globalConfig_);
          subjects_.emplace(std::make_pair(index++, std::move(data))); // ...to add an entry to our "subjects" field...
        }

        this->collectSubjects(); // ...then produce output for all the "subjects" that we just stored
        assert(pseudsToReport_.empty());
      }

      void processResult(const pep::EnumerateAndRetrieveResult& ear) {
        dataCount_++;
        auto existing = subjects_.find(ear.localPseudonymsIndex_);
        if (existing == subjects_.cend()) {
          if (!groupOutput_) {
            collectSubjects();
          }
          [[maybe_unused]] auto emplaced = subjects_.emplace(ear.localPseudonymsIndex_, SubjectData(ear, printMetadata_, globalConfig_));
          assert(emplaced.second);
        }
        else {
          existing->second.add(ear);
        }
      }

      void printQueryInfo() {
          std::ostringstream out;

          out << "Listed " << dataCount_ << " results for: ";
          if (earOpts_.columnGroups.size() > 0 || earOpts_.columns.size() == 0) {
              out << earOpts_.columnGroups.size() << " Column Group(s) ";
          }
          if (earOpts_.columns.size() > 0) {
              out << earOpts_.columns.size() << " Column(s) ";
          }
          out << "and ";
          if (earOpts_.pps.size() > 0) {
              out << earOpts_.pps.size() << " Participant(s) ";
          }
          if (earOpts_.groups.size() > 0 || earOpts_.pps.size() == 0) {
              out << earOpts_.groups.size() << " Participant Group(s)";
          }
          out << std::endl;

          std::cerr << std::move(out).str();
      }
    };
    auto ctx = std::make_shared<Context>(this->getParameterValues());

    return this->executeEventLoopFor([ctx](std::shared_ptr<pep::CoreClient> client) {
      return MultiCellQuery::GetPps(ctx->parameterValues_, client)
        .op(pep::RxToVector())
      .as_dynamic() // Reduce compiler memory usage
      .flat_map([client, ctx](std::shared_ptr<std::vector<pep::PolymorphicPseudonym>> all_pps){
        ctx->earOpts_.groups = MultiCellQuery::GetParticipantGroups(ctx->parameterValues_);
        ctx->earOpts_.pps = *all_pps;
        ctx->earOpts_.columnGroups = MultiCellQuery::GetColumnGroups(ctx->parameterValues_);
        ctx->earOpts_.columns = MultiCellQuery::GetColumns(ctx->parameterValues_);

        if (ctx->parameterValues_.has("no-inline-data")) {
          ctx->earOpts_.includeData = false;
        } else {
          ctx->earOpts_.includeData = true;
          ctx->earOpts_.dataSizeLimit = ctx->parameterValues_.get<uint64_t>("inline-data-size-limit");
        }
        ctx->earOpts_.forceTicket = true;
        ctx->earOpts_.includeAccessGroupPseudonyms = ctx->parameterValues_.has("local-pseudonyms");
        ctx->printMetadata_ = ctx->parameterValues_.has("metadata");
        ctx->groupOutput_ = ctx->parameterValues_.has("group-output");
        ctx->format_ = ctx->parameterValues_.get<std::string>("format");

        // Only fetch GlobalConfiguration if we need it for brief local pseudonyms
        rxcpp::observable<pep::FakeVoid> configObservable;
        if (ctx->earOpts_.includeAccessGroupPseudonyms) {
          configObservable = client->getGlobalConfiguration().map([ctx](std::shared_ptr<pep::GlobalConfiguration> gc) { 
              ctx->globalConfig_ = gc; 
              return pep::FakeVoid(); 
            });
        } else {
          configObservable = rxcpp::observable<>::just(pep::FakeVoid());
        }

        return configObservable.flat_map([ctx, client](pep::FakeVoid) {

        pep::RequestTicket2Opts tOpts;
        tOpts.pps = ctx->earOpts_.pps;
        tOpts.columns = ctx->earOpts_.columns;
        tOpts.columnGroups = ctx->earOpts_.columnGroups;
        tOpts.participantGroups = ctx->earOpts_.groups;
        tOpts.modes = {ctx->earOpts_.includeData ? "read" : "read-meta"};
        tOpts.includeAccessGroupPseudonyms = ctx->earOpts_.includeAccessGroupPseudonyms;
        return pep::cli::TicketFile::GetTicket(*client, ctx->parameterValues_, tOpts)
        .flat_map([client, ctx](pep::IndexedTicket2 ticket)
            -> rxcpp::observable<pep::EnumerateAndRetrieveResult> {
          ctx->earOpts_.ticket = std::make_shared<pep::IndexedTicket2>(
              std::move(ticket));
          if (ctx->parameterValues_.has("show-dataless")) {
            auto pseuds = ctx->earOpts_.ticket->openTicketWithoutCheckingSignature()->accessSubjects_;
            std::transform(pseuds.begin(), pseuds.end(), std::inserter(ctx->pseudsToReport_, ctx->pseudsToReport_.begin()), [](const pep::LocalPseudonyms& lps) {
              return std::make_pair(lps.polymorphic_, lps.accessGroup_);
              });
          }
          return client->enumerateAndRetrieveData2(ctx->earOpts_);
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
        auto tree = pep::structuredOutput::Tree::FromPropertyTree(ctx->results_);
        if (ctx->format_ == "json") {
          pep::structuredOutput::json::append(std::cout, tree) << std::endl;
        } else {
          pep::structuredOutput::yaml::append(std::cout, tree) << std::endl;
        }
        
        ctx->printQueryInfo();
        if (ctx->hasPrintedData_) {
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
