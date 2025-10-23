#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>

#include <pep/application/Application.hpp>
#include <pep/client/Client.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/async/RxBeforeCompletion.hpp>
#include <pep/async/RxRequireCount.hpp>
#include <pep/async/RxInstead.hpp>
#include <pep/structure/GlobalConfiguration.hpp>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <rxcpp/operators/rx-concat_map.hpp>
#include <rxcpp/operators/rx-filter.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-on_error_resume_next.hpp>
#include <rxcpp/operators/rx-tap.hpp>

#include <filesystem>

using namespace pep::cli;
namespace pt = boost::property_tree;

namespace {

struct participantData {
  uint32_t localPseudonymsIndex;
  pt::ptree data;
};

using datalist = std::vector<participantData>;

struct studyData {
  std::unordered_map<std::string, datalist> steps;
  std::unordered_map<std::string, datalist> reports;
};

struct castorData {
  std::unordered_map<std::string, studyData> studies;
  std::unordered_map<uint32_t, std::string> participantIds;
};

const std::string castorColumnPrefix = "Castor.";
const size_t castorColumnPrefixLength = castorColumnPrefix.length();

class CommandCastor : public ChildCommandOf<CliApplication> {
public:
  explicit CommandCastor(CliApplication& parent)
    : ChildCommandOf<CliApplication>("castor", "Commands to work with castor", parent) {
  }

private:
  class CommandCastorExport : public ChildCommandOf<CommandCastor> {
  public:
    explicit CommandCastorExport(CommandCastor& parent)
      : ChildCommandOf<CommandCastor>("export", "Export castor data as csv", parent) {
    }

  private:
    const char csvNewline = '\n';
    std::string csvSeparator;

    std::string csvEscape(std::string value) {
      size_t quotePos = value.find('"');
      bool useQuotes = value.find(csvSeparator) != std::string::npos
        || value.find(' ') != std::string::npos
        || value.find('\n') != std::string::npos
        || value.find('\r') != std::string::npos
        || quotePos != std::string::npos;
      while (quotePos != std::string::npos) {
        value.replace(quotePos, 1, "\"\"");
        quotePos = value.find('"', quotePos + 2);
      }
      if (useQuotes)
        value = '"' + value + '"';
      return value;
    }

    void writeDataFiles(const std::unordered_map<std::string, datalist>& tables, const std::unordered_map<uint32_t, std::string>& participantIds, const std::filesystem::path& dir) {
      std::filesystem::create_directories(dir);

      for (const auto& [tablename, table] : tables) {
        std::vector<std::string> rows;
        std::vector<std::string> columns;
        rows.reserve(table.size());
        for (const auto& row : table) {
          const std::string& participantId = participantIds.at(row.localPseudonymsIndex);
          size_t added = AddMissingColumns(columns, row.data);
          if (added > 0) {
            std::string commas;
            commas.reserve(added * csvSeparator.length());
            for (size_t i = 0; i < added; i++) {
              commas.append(csvSeparator);
            }
            for (std::string& row : rows) {
              row.append(commas);
            }
          }

          std::string newRow = participantId;
          for (const std::string& column : columns) {
            newRow.append(csvSeparator);
            newRow.append(csvEscape(row.data.get<std::string>(column, "")));
          }
          rows.push_back(std::move(newRow));
        }

        std::string stepPath = (dir / tablename).string() + ".csv";
        std::ofstream os(stepPath, std::ios::out | std::ios::binary);
        if (!os.is_open()) {
          throw std::runtime_error("Could not open file " + stepPath);
        }
        os << "participantIdentifier";
        for (const std::string& column : columns) {
          os << csvSeparator << csvEscape(column);
        }
        os << csvNewline;
        for (const std::string& row : rows) {
          os << row << csvNewline;
        }
        os.close();
      }
    }

    static size_t AddMissingColumns(std::vector<std::string>& columns, const pt::ptree& ptree) {
      columns.reserve(ptree.size());
      size_t startSize = columns.size();
      for (const auto& entry : ptree) {
        if (std::find(columns.begin(), columns.end(), entry.first) == columns.end()) {
          columns.push_back(entry.first);
        }
      }
      return columns.size() - startSize;
    }

  protected:
    pep::commandline::Parameters getSupportedParameters() const override {
      return ChildCommandOf<CommandCastor>::getSupportedParameters()
        + pep::commandline::Parameter("output-directory", "Directory to write the exported CSV files to").shorthand('o').value(pep::commandline::Value<std::filesystem::path>()
          .directory().defaultsTo("castor-export"))
        + pep::commandline::Parameter("force", "Removes output directory if present").shorthand('f')
        + pep::commandline::Parameter("separator", "Column separator to be used").shorthand('s').value(pep::commandline::Value<std::string>().defaultsTo(";", "semicolon"));
    }

    int execute() override {
      const auto& values = this->getParameterValues();

      csvSeparator = values.get<std::string>("separator");

      auto dir = values.get<std::filesystem::path>("output-directory");

      if (std::filesystem::exists(dir) && values.has("force")) {
        LOG(LOG_TAG, pep::info) << "Output directory " << dir << " exists.  Removing ..."
                  << std::endl;
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
      }

      if (!std::filesystem::exists(dir)) {
        LOG(LOG_TAG, pep::info) << "Output directory " << dir << " does not exist.  "
                  << "Creating ..." << std::endl;
        std::filesystem::create_directories(dir);
      }

      if (!std::filesystem::is_directory(dir)) {
        LOG(LOG_TAG, pep::error) << "output directory " << dir
              << " is not a directory" << std::endl;
        return 5;
      } else {
        if (std::filesystem::directory_iterator(dir) != std::filesystem::directory_iterator()) {
          LOG(LOG_TAG, pep::error) << "output directory " << dir << " is not empty"
                    << std::endl;
          return 5;
        }
      }

      return this->executeEventLoopFor([this, dir](std::shared_ptr<pep::CoreClient> client) {
        pep::enumerateAndRetrieveData2Opts earOpts;
        earOpts.groups = {"*"};
        earOpts.columnGroups = {"Castor"};
        earOpts.columns = {"ParticipantIdentifier"};
        earOpts.includeData = true;
        earOpts.dataSizeLimit = 0;

        return client->enumerateAndRetrieveData2(earOpts).reduce(
          castorData(),
          [](castorData data, pep::EnumerateAndRetrieveResult earResult) {
            if(earResult.mColumn == "ParticipantIdentifier") {
              data.participantIds.emplace(earResult.mLocalPseudonymsIndex, earResult.mData);
            }
            else if(earResult.mColumn.starts_with(castorColumnPrefix)) {
              std::string studyName = earResult.mColumn.substr(castorColumnPrefixLength,
                earResult.mColumn.find_first_of('.', castorColumnPrefixLength) - castorColumnPrefixLength);

              auto& study = data.studies[studyName];
              std::istringstream iss(earResult.mData);
              pt::ptree dataTree;
              pt::read_json(iss, dataTree);

              if(auto crf = dataTree.get_child_optional("crf")) {
                study.steps[earResult.mColumn].push_back({earResult.mLocalPseudonymsIndex, *crf});
              }
              else {
                LOG(LOG_TAG, pep::warning) << "warning: Castor data is malformed. Missing crf data" << std::endl;
              }
              if(auto reports = dataTree.get_child_optional("reports")) {
                if(!reports->empty()) {
                  for(const auto& [reportname, report] : *reports) {
                    for(const auto& [rdiName, repeatingDataInstance] : report) {
                      if(rdiName != "") {
                        LOG(LOG_TAG, pep::warning) << "warning: Castor data is malformed. Report instances should be an array without keys" << std::endl;
                      }
                      else {
                        study.reports[earResult.mColumn + "." + reportname].push_back({earResult.mLocalPseudonymsIndex, repeatingDataInstance});
                      }
                    }
                  }
                }
              }
              else {
                LOG(LOG_TAG, pep::warning) << "warning: Castor data is malformed. Missing reports data" << std::endl;
              }
            }
            return data;
          },
          [](castorData data) { return data; }
        ).map(
          [this, dir](castorData data){
            for(const auto& [studyname, study] : data.studies) {
              const std::filesystem::path stepsdir = dir / studyname / "steps";
              const std::filesystem::path reportsdir = dir / studyname / "reports";

              this->writeDataFiles(study.steps, data.participantIds, stepsdir);
              this->writeDataFiles(study.reports, data.participantIds, reportsdir);
            }
            LOG(LOG_TAG, pep::info) << "   ... done!" << std::endl;
            return pep::FakeVoid();
          });
        });
    }
  };

  class CommandCastorProcessImportColumns : public ChildCommandOf<CommandCastor> {
  protected:
    explicit CommandCastorProcessImportColumns(const std::string& name, const std::string& description, CommandCastor& parent)
      : ChildCommandOf<CommandCastor>(name, description, parent) {
    }

    pep::commandline::Parameters getSupportedParameters() const override {
      return ChildCommandOf<CommandCastor>::getSupportedParameters()
        + pep::commandline::Parameter("sp-column", "Column containing the short pseudonym for the Castor study to process").shorthand('s').value(pep::commandline::Value<std::string>().required())
        + pep::commandline::Parameter("answer-set-count", "Number of answer sets. Required when processing a SURVEY-type study from which all surveys are imported").value(pep::commandline::Value<unsigned>());
    }

    int processImportColumns(bool provide, bool remaining) {
      if (provide) {
        assert(remaining);
      }

      const auto& values = this->getParameterValues();
      auto studySp = values.get<std::string>("sp-column");
      std::optional<unsigned> answerSetCount;
      if (values.has("answer-set-count")) {
        answerSetCount = values.get<unsigned>("answer-set-count");
      }

      struct ColumnStatus {
        std::string name;
        std::optional<bool> exists;
        std::optional<bool> grouped;
      };

      return this->executeEventLoopFor([provide, remaining, studySp, answerSetCount](std::shared_ptr<pep::Client> client) {
        auto required = client->listCastorImportColumns(studySp, answerSetCount)
          .map([](const std::string& name) {return ColumnStatus{ name, std::nullopt, std::nullopt }; });
        rxcpp::observable<ColumnStatus> process = required;

        if (remaining) {
          struct CurrentConfig {
            std::unordered_set<std::string> existing;
            std::unordered_set<std::string> grouped;
          };

          process = client->amaQuery(pep::AmaQuery{})
            .op(pep::RxGetOne())
            .map([](const pep::AmaQueryResponse& response) {
            auto config = std::make_shared<CurrentConfig>();
            for (const auto& column : response.mColumns) {
              [[maybe_unused]] auto emplaced = config->existing.emplace(column.mName);
              assert(emplaced.second);
            }
            const auto& castorGroup = std::find_if(response.mColumnGroups.cbegin(), response.mColumnGroups.cend(), [](const pep::AmaQRColumnGroup& group) {return group.mName == "Castor"; });
            if (castorGroup != response.mColumnGroups.cend()) {
              for (const auto& column : castorGroup->mColumns) {
                [[maybe_unused]] auto emplaced = config->grouped.emplace(column);
                assert(emplaced.second);
              }
            }
            return config;
            })
            .concat_map([required](std::shared_ptr<CurrentConfig> config) {
              return required
                .map([config](ColumnStatus column) {
                column.exists = (config->existing.find(column.name) != config->existing.cend());
                column.grouped = (config->grouped.find(column.name) != config->grouped.cend());
                return column;
                  });
              })
            .filter([](const ColumnStatus& column) {return !*column.grouped; });
        }

        if (provide) {
          assert(remaining);
          process = process
            .concat_map([client](const ColumnStatus& column) {
            assert(!column.grouped.value_or(true));
            rxcpp::observable<pep::FakeVoid> created;
            if (*column.exists) {
              created = rxcpp::observable<>::just(pep::FakeVoid());
            }
            else {
              created = client->amaCreateColumn(column.name);
            }
            return created
              .flat_map([client, name = column.name](pep::FakeVoid) {return client->amaAddColumnToGroup(name, "Castor"); })
              .map([column](pep::FakeVoid) { return column; });
              });
        }

        return process.tap(
            [](const ColumnStatus& column) {
              std::cout << column.name;
              assert(column.exists.has_value() == column.grouped.has_value());
              if (column.exists.has_value()) {
                assert(!*column.grouped);
                std::cout << " (";
                if (!*column.exists) {
                  std::cout << "create and ";
                }
                std::cout << "add to 'Castor' group)";
              }
              std::cout << std::endl;
            },
            [](std::exception_ptr) { /* do nothing */},
            []() { LOG(LOG_TAG, pep::info) << "   ... done!" << std::endl; }
          ).op(pep::RxInstead(pep::FakeVoid()));
        });
    }
  };

  class CommandCastorListImportColumns : public CommandCastorProcessImportColumns {
  public:
    explicit CommandCastorListImportColumns(CommandCastor& parent)
      : CommandCastorProcessImportColumns("list-import-columns", "List a study's imported columns", parent) {
    }

  protected:
    inline std::optional<std::string> getRelativeDocumentationUrl() const override { return "using-pepcli#castor-list-import-columns"; }

    pep::commandline::Parameters getSupportedParameters() const override {
      return CommandCastorProcessImportColumns::getSupportedParameters()
        + pep::commandline::Parameter("remaining", "Limit to columns that do not exist (yet)").shorthand('r');
    }

    int execute() override {
      return this->processImportColumns(false, this->getParameterValues().has("remaining"));
    }
  };

  class CommandCastorCreateImportColumns : public CommandCastorProcessImportColumns {
  public:
    explicit CommandCastorCreateImportColumns(CommandCastor& parent)
      : CommandCastorProcessImportColumns("create-import-columns", "Create imported columns", parent) {
    }

  protected:
    inline std::optional<std::string> getRelativeDocumentationUrl() const override { return "using-pepcli#castor-create-import-columns"; }

    pep::commandline::Parameters getSupportedParameters() const override {
      return CommandCastorProcessImportColumns::getSupportedParameters()
        + pep::commandline::Parameter("dry", "Perform a dry run (only list columns instead of also creating them)");
    }

    int execute() override {
      return this->processImportColumns(!this->getParameterValues().has("dry"), true);
    }
  };

  class CommandCastorListSpColumns : public ChildCommandOf<CommandCastor> {
  public:
    explicit CommandCastorListSpColumns(CommandCastor& parent)
      : ChildCommandOf<CommandCastor>("list-sp-columns", "List Castor short pseudonym columns", parent) {
    }

  protected:
    inline std::optional<std::string> getRelativeDocumentationUrl() const override { return "using-pepcli#castor-list-sp-columns"; }

    pep::commandline::Parameters getSupportedParameters() const override {
      return ChildCommandOf<CommandCastor>::getSupportedParameters()
        + pep::commandline::Parameter("imported-only", "Only list SP columns that are processed during Castor import");
    }

    int execute() override {
      return this->executeEventLoopFor([importedOnly = this->getParameterValues().has("imported-only")](std::shared_ptr<pep::CoreClient> client) {
        rxcpp::observable<pep::ShortPseudonymDefinition> sps = client->getGlobalConfiguration()
          .flat_map([](std::shared_ptr<pep::GlobalConfiguration> config) {return rxcpp::observable<>::iterate(config->getShortPseudonyms()); })
          .filter([](const pep::ShortPseudonymDefinition& sp) {return sp.getCastor(); });
        if (importedOnly) {
          sps = sps.filter([](const pep::ShortPseudonymDefinition& sp) {return !sp.getCastor()->getStorageDefinitions().empty(); });
        }

        return sps.tap([](const pep::ShortPseudonymDefinition& sp) {
          std::cout << sp.getColumn().getFullName()
            << std::endl;
        }, [](std::exception_ptr) { /* do nothing */},
            []() {
          LOG(LOG_TAG, pep::info) << "   ... done!" << std::endl;
        }).op(pep::RxInstead(pep::FakeVoid()));
        });
    }
  };

  class CommandCastorColumnNameMapping : public ChildCommandOf<CommandCastor> {
  public:
    explicit CommandCastorColumnNameMapping(CommandCastor& parent)
      : ChildCommandOf<CommandCastor>("column-name-mapping", "Manage (import) column name mappings", parent) {
    }

  private:
    class ColumnNameMappingSubCommand : public ChildCommandOf<CommandCastorColumnNameMapping> {
    private:
      static pep::FakeVoid ReportColumnNameMappings(std::shared_ptr<pep::ColumnNameMappings> mappings) {
        auto entries = mappings->getEntries();
        std::sort(entries.begin(), entries.end(), [](const pep::ColumnNameMapping& lhs, const pep::ColumnNameMapping& rhs) { return lhs.original.getValue() < rhs.original.getValue(); });
        for (const auto& entry : entries) {
          std::cout << std::quoted(entry.original.getValue()) << " --> " << std::quoted(entry.mapped.getValue()) << std::endl;
        }
        return pep::FakeVoid();
      }

    protected:
      ColumnNameMappingSubCommand(const std::string& name, const std::string& description, CommandCastorColumnNameMapping& parent)
        : ChildCommandOf<CommandCastorColumnNameMapping>(name, description, parent) {
      }

      virtual rxcpp::observable<std::shared_ptr<pep::ColumnNameMappings>> getAffectedMappings(std::shared_ptr<pep::CoreClient> client) = 0;

      int execute() override {
        return this->executeEventLoopFor([this](std::shared_ptr<pep::CoreClient> client) {
          return this->getAffectedMappings(client)
            .map(ReportColumnNameMappings)
            .op(pep::RxBeforeCompletion(
              []() {
                LOG(LOG_TAG, pep::info) << "   ... done!" << std::endl;
              }));
          });
      }
    };

    class ColumnNameMappingListCommand : public ColumnNameMappingSubCommand {
    public:
      explicit ColumnNameMappingListCommand(CommandCastorColumnNameMapping& parent)
        : ColumnNameMappingSubCommand("list", "List column name mappings", parent) {
      }

    protected:
      rxcpp::observable<std::shared_ptr<pep::ColumnNameMappings>> getAffectedMappings(std::shared_ptr<pep::CoreClient> client) override {
        return client->getColumnNameMappings();
      }
    };

    class SingleColumnNameMappingCommand : public ColumnNameMappingSubCommand {
    protected:
      SingleColumnNameMappingCommand(const std::string& name, const std::string& description, CommandCastorColumnNameMapping& parent)
        : ColumnNameMappingSubCommand(name, description, parent) {
      }

      pep::commandline::Parameters getSupportedParameters() const override {
        return ColumnNameMappingSubCommand::getSupportedParameters()
          + pep::commandline::Parameter("castor", "The name defined in Castor").value(pep::commandline::Value<std::string>().positional().required());
      }

      pep::ColumnNameSection getSpecifiedCastorColumnNameSection() const {
        return pep::ColumnNameSection::FromRawString(this->getParameterValues().get<std::string>("castor"));
      }
    };

    class ColumnNameMappingReadCommand : public SingleColumnNameMappingCommand {
    public:
      explicit ColumnNameMappingReadCommand(CommandCastorColumnNameMapping& parent)
        : SingleColumnNameMappingCommand("read", "Retrieve a column name mapping", parent) {
      }

    protected:
      rxcpp::observable<std::shared_ptr<pep::ColumnNameMappings>> getAffectedMappings(std::shared_ptr<pep::CoreClient> client) override {
        return client->readColumnNameMapping(this->getSpecifiedCastorColumnNameSection());
      }
    };

    class ColumnNameMappingDeleteCommand : public SingleColumnNameMappingCommand {
    public:
      explicit ColumnNameMappingDeleteCommand(CommandCastorColumnNameMapping& parent)
        : SingleColumnNameMappingCommand("delete", "Remove a column name mapping", parent) {
      }

    protected:
      rxcpp::observable<std::shared_ptr<pep::ColumnNameMappings>> getAffectedMappings(std::shared_ptr<pep::CoreClient> client) override {
        return client->deleteColumnNameMapping(this->getSpecifiedCastorColumnNameSection())
          .map([](pep::FakeVoid) {return std::make_shared<pep::ColumnNameMappings>(pep::ColumnNameMappings({})); });
      }
    };

    class ColumnNameMappingWriteCommand : public SingleColumnNameMappingCommand {
    protected:
      ColumnNameMappingWriteCommand(const std::string& name, const std::string& description, CommandCastorColumnNameMapping& parent)
        : SingleColumnNameMappingCommand(name, description, parent) {
      }

      pep::commandline::Parameters getSupportedParameters() const override {
        return SingleColumnNameMappingCommand::getSupportedParameters()
          + pep::commandline::Parameter("pep", "The replacement (column) name used in PEP").value(pep::commandline::Value<std::string>().positional().required());
      }

      pep::ColumnNameMapping getSpecifiedColumnNameMapping() const {
        auto castor = this->getSpecifiedCastorColumnNameSection();
        auto pep = pep::ColumnNameSection::FromRawString(this->getParameterValues().get<std::string>("pep"));
        return pep::ColumnNameMapping{ castor, pep };
      }
    };

    class ColumnNameMappingCreateCommand : public ColumnNameMappingWriteCommand {
    public:
      explicit ColumnNameMappingCreateCommand(CommandCastorColumnNameMapping& parent)
        : ColumnNameMappingWriteCommand("create", "Create a column name mapping", parent) {
      }

    protected:
      rxcpp::observable<std::shared_ptr<pep::ColumnNameMappings>> getAffectedMappings(std::shared_ptr<pep::CoreClient> client) override {
        return client->createColumnNameMapping(this->getSpecifiedColumnNameMapping());
      }
    };

    class ColumnNameMappingUpdateCommand : public ColumnNameMappingWriteCommand {
    public:
      explicit ColumnNameMappingUpdateCommand(CommandCastorColumnNameMapping& parent)
        : ColumnNameMappingWriteCommand("update", "Set a new PEP name for an existing Castor name", parent) {
      }

    protected:
      rxcpp::observable<std::shared_ptr<pep::ColumnNameMappings>> getAffectedMappings(std::shared_ptr<pep::CoreClient> client) override {
        return client->updateColumnNameMapping(this->getSpecifiedColumnNameMapping());
      }
    };

  protected:
    inline std::optional<std::string> getRelativeDocumentationUrl() const override { return "using-pepcli#castor-column-name-mapping"; }

    std::vector<std::shared_ptr<pep::commandline::Command>> createChildCommands() override {
      std::vector<std::shared_ptr<pep::commandline::Command>> result;
      result.push_back(std::make_shared<ColumnNameMappingListCommand>(*this));
      result.push_back(std::make_shared<ColumnNameMappingCreateCommand>(*this));
      result.push_back(std::make_shared<ColumnNameMappingReadCommand>(*this));
      result.push_back(std::make_shared<ColumnNameMappingUpdateCommand>(*this));
      result.push_back(std::make_shared<ColumnNameMappingDeleteCommand>(*this));
      return result;
    }

  };

  protected:
    inline std::optional<std::string> getRelativeDocumentationUrl() const override { return "using-pepcli#castor"; }

    std::vector<std::shared_ptr<pep::commandline::Command>> createChildCommands() override {
      std::vector<std::shared_ptr<pep::commandline::Command>> result;
      result.push_back(std::make_shared<CommandCastorExport>(*this));
      result.push_back(std::make_shared<CommandCastorListImportColumns>(*this));
      result.push_back(std::make_shared<CommandCastorCreateImportColumns>(*this));
      result.push_back(std::make_shared<CommandCastorListSpColumns>(*this));
      result.push_back(std::make_shared<CommandCastorColumnNameMapping>(*this));
      return result;
    }
};

}

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandCastor(CliApplication& parent) {
  return std::make_shared<CommandCastor>(parent);
}
