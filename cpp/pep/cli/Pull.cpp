#include <pep/utils/Win32Api.hpp>

#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>

#include <boost/range/iterator_range.hpp>
#include <boost/algorithm/string/join.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <thread>

#include <pep/cli/DownloadDirectory.hpp>
#include <pep/cli/Export.hpp>
#include <pep/cli/structuredoutput/TableFromDownloadDirectory.hpp>
#include <pep/cli/MultiCellQuery.hpp>
#include <pep/core-client/CoreClient.hpp>
#include <pep/structuredoutput/Csv.hpp>
#include <pep/structuredoutput/FormatFlags.hpp>
#include <pep/structuredoutput/Json.hpp>
#include <pep/async/RxBeforeCompletion.hpp>
#include <pep/async/RxToVector.hpp>

#include <rxcpp/operators/rx-concat.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-zip.hpp>

using namespace pep::cli;
using namespace std::chrono_literals;
namespace so = pep::structuredOutput;

namespace {
constexpr auto SUPPORTED_EXPORT_FORMATS = so::FormatFlags::csv | so::FormatFlags::json | so::FormatFlags::yaml;

struct Context {
  bool update{ false };
  bool force{ false };
  bool resume{ false };
  bool updateFormat{ false };
  bool allAccessible{ false };
  bool applyFileExtensions{ DownloadDirectory::APPLY_FILE_EXTENSIONS_BY_DEFAULT };
  std::string outputDirectory;
  std::string tempDirectory;
  DownloadDirectory::PullOptions options;
  DownloadDirectory::ContentSpecification content;
  std::shared_ptr<pep::Progress> progress = pep::Progress::Create(3U);
  pep::EventSubscription progressSubscription;
  so::FormatFlags exportFormats = so::FormatFlags::none;
};

/*!
* \brief Will a saved configuration file be used for this command?
*/
bool UsesSavedConfig(const std::shared_ptr<Context> &ctx) {
  return ctx->update || ctx->updateFormat || ctx->resume;
}

/*!
* \brief Create a working copy of the source in which changes can safely be made without losing the original data.
* \param source Path to the original file or directory
* \param dest Path to the directory in which to place the copies
*/
void HardlinkFolders(std::filesystem::path source, std::filesystem::path dest) {
  for (const auto& entry : std::filesystem::directory_iterator(source)) {
    if (!std::filesystem::is_directory(entry)) {
      auto filename = entry.path().filename();
      std::filesystem::path newPath = dest / filename;
      std::filesystem::create_hard_link(entry, newPath);
    }
    else {
      std::filesystem::path appendedSource = source / entry.path().filename();
      std::filesystem::path appendedDest = dest / entry.path().filename();
      std::filesystem::create_directory(appendedDest);
      HardlinkFolders(appendedSource, appendedDest);
    }
  }
}

/*!
* \brief Changes the names of the participant- and metadata directories from the long User Pseudonyms to the shorter Participant Alias.
* \param directory Path to the output directory
* \param globalConfig Shared pointer to the GlobalConfiguration
*/
void UpdateFormat(std::filesystem::path directory, std::shared_ptr<pep::GlobalConfiguration> globalConfig) {
  auto metadataDir = directory / DownloadMetadata::GetDirectoryName();
  for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory)) {
    std::string entryName = entry.path().filename().string();
    auto metaPath = metadataDir / entryName;
    if (std::filesystem::is_directory(entry)
      && !std::filesystem::equivalent(entry.path(), metadataDir)
      && !globalConfig->getUserPseudonymFormat().matches(entryName)
      && std::filesystem::exists(metaPath)
      && entryName.size() == pep::LocalPseudonym::TextLength()) {
      auto lp = pep::LocalPseudonym::FromText(entryName);
      std::string participantAlias = globalConfig->getUserPseudonymFormat().makeUserPseudonym(lp);
      std::filesystem::rename(entry.path(), entry.path().parent_path() / participantAlias);
      std::filesystem::rename(metaPath, metaPath.parent_path() / participantAlias);
    }
  }
}

/*!
* \brief Print the current stage of the download process to std::cout
* \param progress The Progress object containing the current state
*/
void ReportProgress(const pep::Progress& progress) {
  auto state = progress.getState();
  assert(!state.empty());
  const auto& top = *state.top();
  if (!top.done()) {
    for (size_t i = 0; i < state.size() - 1U; ++i) {
      std::cout << "    ";
    }
    std::cout << state.top()->describe() << '\n';
  }
}

/*!
* \brief Assure that the temp directory exists and if needed, is reset to match the outputDirectory.
* \param ctx Shared pointer to the Context
*/
void prepareTempDirectory(const std::shared_ptr<Context> &ctx) {
  // Existing temp directory should be left intact only if --resume is specified
  if (std::filesystem::exists(ctx->tempDirectory) && !ctx->resume) {
    // Updating but not resuming: we'll need a fresh temp directory
    std::filesystem::remove_all(ctx->tempDirectory);
  }

  if (!std::filesystem::exists(ctx->tempDirectory)) {
    std::filesystem::create_directory(ctx->tempDirectory);
    HardlinkFolders(ctx->outputDirectory, ctx->tempDirectory);
  }
}

/*!
* \brief Many flags and switches can not be simultaneously set. This function guards against all incompatible combinations.
* \param ctx Shared pointer to the Context
*/
void checkContextSettings(const std::shared_ptr<Context> &ctx) {
  if (ctx->force && ctx->options.assumePristine) {
    throw std::runtime_error("Options --force and --assume-pristine cannot be used together - specify either one or the other");
  }
  if (ctx->updateFormat && ctx->update) {
    throw std::runtime_error("Options --update-format and --update cannot be used together - specify either one or the other");
  }
  if (ctx->updateFormat && ctx->resume) {
    throw std::runtime_error("Options --update-format and --resume cannot be used together - specify either one or the other");
  }
  if (ctx->updateFormat && ctx->options.assumePristine) {
    throw std::runtime_error("Options --update-format and --assume-pristine cannot be used together - specify either one or the other");
  }

  ctx->progress->advance("Checking local data");
  if (ctx->update || ctx->updateFormat) {
    // Existing temp directory should be left intact only if --resume is specified
    if (std::filesystem::exists(ctx->tempDirectory) && !ctx->resume) {
      // User wants to update and we'll discard the temp directory: we'll need an output directory to work with
      if (!std::filesystem::exists(ctx->outputDirectory)) {
        throw std::runtime_error("Didn't find a source directory with name: " + ctx->outputDirectory + " to update");
      }
      // Verify that user really wants temp directory to be discarded
      if (!ctx->force && !std::filesystem::is_empty(ctx->tempDirectory)) {
        throw std::runtime_error("Temporary download directory " + ctx->tempDirectory +
          " already exists. Specify --force to clear this directory or --resume to resume the download from this directory");
      }
    }
  }
  else if (ctx->options.assumePristine || ctx->resume) {
    throw std::runtime_error("Options --assume-pristine and --resume may only be passed when the --update option is also passed");
  }
  else {
    if (!ctx->force) {
      if (std::filesystem::exists(ctx->outputDirectory) && !std::filesystem::is_empty(ctx->outputDirectory)) {
        throw std::runtime_error("Output directory " + ctx->outputDirectory + " already exists. Specify --force to clear the directory and download anyway");
      }
      if (std::filesystem::exists(ctx->tempDirectory) && !std::filesystem::is_empty(ctx->tempDirectory)) {
        throw std::runtime_error("Temporary download directory " + ctx->tempDirectory + " already exists. Specify --force to clear the directory and download anyway");
      }
    }
  }
}

so::FormatFlags ParseExportFormats(const std::vector<std::string>& formatNames) {
  static_assert(
      SUPPORTED_EXPORT_FORMATS == (so::FormatFlags::csv | so::FormatFlags::json | so::FormatFlags::yaml),
      "formats handled in this function must mirror the SUPPORTED_EXPORT_FORMATS");

  auto flags = so::FormatFlags::none;
  for (const auto& name : formatNames) {
    if (name == "csv") { flags |= so::FormatFlags::csv; }
    else if (name == "json") { flags |= so::FormatFlags::json; }
    else if (name == "yaml") { flags |= so::FormatFlags::yaml; }
    else {
      const auto supported = so::ToSingleString(SUPPORTED_EXPORT_FORMATS, ", ");
      throw std::runtime_error("\"" + name + "\" is not a valid export format. Supported formats are: " + supported);
    }
  }
  return flags;
}

/*!
* \brief Based on the values given by the user, create a Context object that contains all required data to perform the download.
* \param client Shared pointer to the pepClient.
* \param values The cli flag and switch values given by the user
*/
rxcpp::observable<std::shared_ptr<Context>> createContext(const std::shared_ptr<pep::CoreClient> client, const pep::commandline::NamedValues& values) {
  auto ctx = std::make_shared<Context>();
  if (values.has("report-progress")) {
    ctx->progressSubscription = ctx->progress->onChange.subscribe(ReportProgress);
  }
  ctx->progress->advance("Constructing query");
  ctx->update = values.has("update");
  ctx->force = values.has("force");
  ctx->resume = values.has("resume");
  ctx->updateFormat = values.has("update-pseudonym-format");
  ctx->outputDirectory = values.get<std::filesystem::path>("output-directory").string();
  ctx->tempDirectory = std::filesystem::path(ctx->outputDirectory).replace_filename(std::filesystem::path(ctx->outputDirectory).filename().string() + "-pending").string();
  ctx->options.assumePristine = values.has("assume-pristine");
  ctx->allAccessible = values.has("all-accessible");
  ctx->exportFormats = ParseExportFormats(values.getOptionalMultiple<std::string>("export"));

  if (values.has("suppress-file-extensions")) {
    if (UsesSavedConfig(ctx)) {
      throw std::runtime_error("Updates process file extensions as specified for the original download - do not suppress file extensions on the command line");
    }
    ctx->applyFileExtensions = false;
  }

  checkContextSettings(ctx);

  // Check command line options directly for -P and the like as we only assign them to ctx below if needed
  if (UsesSavedConfig(ctx) && (MultiCellQuery::IsNonEmpty(values) || ctx->allAccessible)) {
    throw std::runtime_error("Updates process the content specified for the original download - do not specify the desired content on the command line");
  }

  if (ctx->allAccessible) {
    if (MultiCellQuery::IsNonEmpty(values)) {
      throw std::runtime_error("Option --all-accessible cannot be used together with other options specifying columns or participants");
    }

    auto& am = *client->getAccessManagerProxy();
    return am.getAccessibleParticipantGroups(true)
      .zip(am.getAccessibleColumns(true, { "read" }))
      .map([ctx](const auto &access) {
      const pep::ParticipantGroupAccess &pga = std::get<0>(access);
      for (const auto& pg : pga.participantGroups) {
        if (std::find(pg.second.begin(), pg.second.end(), "access") != pg.second.end())
        {
          ctx->content.groups.push_back(pg.first);
        }
      }
      const pep::ColumnAccess &ca = std::get<1>(access);
      ctx->content.columnGroups.reserve(ca.columnGroups.size());
      for (const auto& cg : ca.columnGroups) {
        assert(std::find(cg.second.modes.begin(), cg.second.modes.end(), "read") != cg.second.modes.end());
        ctx->content.columnGroups.push_back(cg.first);
      }
      if (ctx->content.groups.empty()) {
        LOG(LOG_TAG, pep::warning) << "No accessible participants - download will contain no data";
      }
      if (ctx->content.columnGroups.empty()) {
        LOG(LOG_TAG, pep::warning) << "No accessible columns - download will contain no data";
      }
      return ctx;
    });
  }
  else {
    if (!UsesSavedConfig(ctx)) {
      bool fullySpecified = true;
      if (!MultiCellQuery::SpecifiesColumns(values)) {
        LOG(LOG_TAG, pep::error) << "No columns specified";
        fullySpecified = false;
      }
      if (!MultiCellQuery::SpecifiesParticipants(values)) {
        LOG(LOG_TAG, pep::error) << "No participants specified";
        fullySpecified = false;
      }
      if (!fullySpecified) {
        throw std::runtime_error("Desired data is not fully specified - download will contain no data");
      }
    }

    ctx->content.groups = MultiCellQuery::GetParticipantGroups(values);
    ctx->content.columnGroups = MultiCellQuery::GetColumnGroups(values);
    ctx->content.columns = MultiCellQuery::GetColumns(values);

    return MultiCellQuery::GetPps(values, client)
      .op(pep::RxToVector())
          .map([ctx](std::shared_ptr<std::vector<pep::PolymorphicPseudonym>> pps) {
            ctx->content.pps = *pps;
            return ctx;
          });
  }
}
/*!
* \brief Based on the settings in the Context, create a DownloadDirectory that will handle all further data downloading and file creation.
* \param ctx shared pointer to the Context
* \param client shared pointer to the pepClient
* \param globalConfig Shared pointer to the GlobalConfiguration
*/
std::shared_ptr<DownloadDirectory> createDownloadDirectory(const std::shared_ptr<Context> ctx, const std::shared_ptr<pep::CoreClient> client, const std::shared_ptr<pep::GlobalConfiguration> globalConfig, bool applyFileExtensions) {
  std::shared_ptr<DownloadDirectory> directory;
  if (ctx->update) {
    prepareTempDirectory(ctx);

    auto progress = ctx->progress;
    auto checkPristine = !ctx->options.assumePristine && !ctx->force;
    if (checkPristine) { // Announce a separate step for each of the multiple directory iterations that we'll perform
      progress = pep::Progress::Create(2U, ctx->progress->push());
      progress->advance("Reading participant data");
    }

    directory = DownloadDirectory::Create(ctx->tempDirectory, globalConfig, progress->push());

    if (checkPristine) {
      progress->advance("Checking directory for changes");
      auto nonpristine = directory->getNonPristineEntries(progress->push());
      if (!nonpristine.empty()) {
        std::vector<std::string> lines;
        lines.reserve(nonpristine.size() + 1);
        lines.emplace_back("Data in output directory " + ctx->outputDirectory + " has changed since last download. Specify --force to discard local changes and update to server version.");

        std::transform(nonpristine.begin(), nonpristine.end(), std::back_inserter(lines), [](const pep::cli::DownloadDirectory::NonPristineEntry& entry) {
          if (!entry.path.has_value()) {
            assert(entry.record.has_value());
            return "Absent file for participant " + entry.record->getParticipant().getLocalPseudonym().text() + ", column " + entry.record->getColumn();
          }
          if (!entry.record.has_value()) {
            assert(entry.path.has_value());
            return std::string("Unknown ") + (is_directory(*entry.path) ? "directory" : "file") + ' ' + entry.path->string();
          }
          assert(!is_directory(*entry.path));
          return "File " + entry.path->string() + " has local changes";
          });
        throw std::runtime_error(boost::join(lines, "\n- "));
      }
    }
    else if (ctx->force) {
      for (auto unknown : directory->getUnknownContents()) {
        remove_all(unknown);
      }
    }
  }
  else {
    // Either the outputDirectory and tempDirectory do not exist/are empty, or --force was specified. Either way, start with a clean slate.
    if (std::filesystem::exists(ctx->outputDirectory)) {
      std::filesystem::remove_all(ctx->outputDirectory);
    }
    if (std::filesystem::exists(ctx->tempDirectory)) {
      std::filesystem::remove_all(ctx->tempDirectory);
    }
    directory = DownloadDirectory::Create(ctx->tempDirectory, client, ctx->content, globalConfig, applyFileExtensions);
  }
  return directory;
}
void cleanUp(std::shared_ptr<Context> ctx) {
  ctx->progress->advanceToCompletion();

  //When updating, remove old data after succesfull pull
  if (ctx->update || ctx->updateFormat) {
    std::filesystem::remove_all(ctx->outputDirectory);

    // The "rename" call below often hangs after our "remove_all", possibly due to file system latency.
    // (Letting the file system catch up while we) sleep reliably gets rid of the problem for me.
    std::this_thread::sleep_for(500ms);
  }

  //Move downloaded data to final destination after succesfull pull
  if (std::filesystem::exists(ctx->outputDirectory)) {
    throw std::runtime_error("Output directory already exists, please remove it before initiating a pull.");
  }
  std::filesystem::rename(ctx->tempDirectory, ctx->outputDirectory);
  LOG(LOG_TAG, pep::info) << "Data downloaded to " << std::filesystem::absolute(ctx->outputDirectory).string();
}

struct ExportContext final {
  std::shared_ptr<pep::GlobalConfiguration> globalConfig;
  std::filesystem::path input_directory;
  bool force;
};

void ExecuteExports(const so::FormatFlags formats, const ExportContext ctx) {
  if (formats == so::FormatFlags::none) { return; }

  const auto downloadDir = DownloadDirectory::Create(ctx.input_directory, ctx.globalConfig, [](std::shared_ptr<const pep::Progress>) {}); // TODO: report (instead of ignore) progress
  const auto table = so::TableFrom(*downloadDir);
  const auto exportAs = [&ctx](const std::string format, std::function<void(std::ofstream&)> write) {
    const auto dest = ctx.input_directory.parent_path() / ("export." + format); // assuming format == file extension
    if (!ctx.force && std::filesystem::exists(dest)) {
      LOG(LOG_TAG, pep::error) << "Export destination \"" + dest.string()
              + "\" already exists, please remove it and then run \"pepcli export " + format + "\".";
      return; // skip this format
    }
    auto stream = std::ofstream{dest};
    LOG(LOG_TAG, pep::info) << "Exporting pulled data as \"" + format + "\" to \"" + dest.string() + "\".";
    write(stream);
  };

  static_assert(
      SUPPORTED_EXPORT_FORMATS == (so::FormatFlags::csv | so::FormatFlags::json | so::FormatFlags::yaml),
      "formats handled in this function must mirror the SUPPORTED_EXPORT_FORMATS");

  if (Contains(formats, so::FormatFlags::csv)) {
    exportAs("csv", [&table](std::ofstream& stream) { so::csv::append(stream, table); });
  }
  if (Contains(formats, so::FormatFlags::json)) {
    exportAs("json", [&table](std::ofstream& stream) { so::json::append(stream, table); });
  }
  if (Contains(formats, so::FormatFlags::json)) {
    exportAs("yaml", [&table](std::ofstream& stream) { so::json::append(stream, table); });
  }
}

class CommandPull : public ChildCommandOf<CliApplication> {
public:
  explicit CommandPull(CliApplication& parent)
  : ChildCommandOf<CliApplication>("pull", "Retrieve files to a local directory", parent) {
  }

protected:
  inline std::optional<std::string> getRelativeDocumentationUrl() const override { return "using-pepcli#pull"; }

  pep::commandline::Parameters getSupportedParameters() const override {
    using pep::commandline::Parameter;
    using pep::commandline::Value;
    return ChildCommandOf<CliApplication>::getSupportedParameters()
      + Parameter("output-directory", "Directory to write files to").shorthand('o').value(Value<std::filesystem::path>()
        .directory().defaultsTo("pulled-data"))
      + Parameter("force", "Overwrite or remove existing data in output directory").shorthand('f')
      + Parameter("resume", "Resume a download from the temporary folder").shorthand('r')
      + Parameter("update", "Updates an existing output directory").shorthand('u')
      + Parameter("assume-pristine", "Don't check data files during update")
      + Parameter("update-pseudonym-format", "Renames directories in your download directory from using long pseudonyms to the shorter participant alias")
      + Parameter("all-accessible", "Download all data to which the current UserGroup has access")
      + MultiCellQuery::Parameters()
      + Parameter("report-progress", "Produce progress status messages")
      + Parameter("suppress-file-extensions", "Don't apply file extensions to downloaded files")
      + Parameter("export", "Add supplementary output in the selected format").value(Value<std::string>()
        .multiple().allow(ToIndividualStrings(SUPPORTED_EXPORT_FORMATS)));
  }

  int execute() override {
    return this->executeEventLoopFor([this](std::shared_ptr<pep::CoreClient> client) {
      const auto& values = this->getParameterValues();
      return createContext(client, values)
      .flat_map([client](std::shared_ptr<Context> ctx) {
        return client->getGlobalConfiguration()
        .flat_map([client, ctx](std::shared_ptr<pep::GlobalConfiguration> globalConfig) {
          if (ctx->updateFormat) {
            ctx->progress->advance("Updating pseudonym format");
            prepareTempDirectory(ctx);
            UpdateFormat(ctx->tempDirectory, globalConfig);
            return rxcpp::observable<pep::FakeVoid>();
          }

          auto directory = createDownloadDirectory(ctx, client, globalConfig, ctx->applyFileExtensions);
          ctx->progress->advance("Downloading");
          return directory->pull(client, ctx->options, ctx->progress->push())
          .op(pep::RxBeforeCompletion([ctx, globalConfig]() {
            cleanUp(ctx);
            ExecuteExports(ctx->exportFormats, {.globalConfig = globalConfig, .input_directory = ctx->outputDirectory, .force = ctx->force}); // TODO: pass existing "directory" so that "ExecuteExports" doesn't have to re-invoke "DownloadDirectory::Create"
          }));
        });
      });
    });
  }
};

} // end anonymous namespace

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandPull(CliApplication& parent) {
  return std::make_shared<CommandPull>(parent);
}
