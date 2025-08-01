#include <pep/utils/Win32Api.hpp>

#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>

#include <boost/range/iterator_range.hpp>

#include <filesystem>
#include <sstream>
#include <thread>

#include <pep/cli/DownloadDirectory.hpp>
#include <pep/cli/MultiCellQuery.hpp>
#include <pep/core-client/CoreClient.hpp>
#include <pep/async/RxUtils.hpp>

#include <rxcpp/operators/rx-concat.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-zip.hpp>

using namespace pep::cli;
using namespace std::chrono_literals;

namespace {
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
  if (ctx->force && ctx->resume) {
    throw std::runtime_error("Options --force and --resume cannot be used together - specify either one or the other");
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

    return client->getAccessibleParticipantGroups(true)
      .zip(client->getAccessibleColumns(true, { "read" }))
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
    directory = DownloadDirectory::Create(ctx->tempDirectory, globalConfig);

    if (!ctx->options.assumePristine && !ctx->force) {
      auto nonpristine = directory->describeFirstNonPristineEntry(ctx->progress->push());
      if (nonpristine != std::nullopt) {
        throw std::runtime_error("Data in output directory " + ctx->outputDirectory + " has changed since last download: " + *nonpristine + ". Specify --force to discard local changes and update to server version");
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


class CommandPull : public ChildCommandOf<CliApplication> {
public:
  explicit CommandPull(CliApplication& parent)
  : ChildCommandOf<CliApplication>("pull", "Retrieve files to a local directory", parent) {
  }

protected:
  inline std::optional<std::string> getRelativeDocumentationUrl() const override { return "using-pepcli#pull"; }

  pep::commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf<CliApplication>::getSupportedParameters()
      + pep::commandline::Parameter("output-directory", "Directory to write files to").shorthand('o').value(pep::commandline::Value<std::filesystem::path>()
        .directory().defaultsTo("pulled-data"))
      + pep::commandline::Parameter("force", "Overwrite or remove existing data in output directory").shorthand('f')
      + pep::commandline::Parameter("resume", "Resume a download from the temporary folder").shorthand('r')
      + pep::commandline::Parameter("update", "Updates an existing output directory").shorthand('u')
      + pep::commandline::Parameter("assume-pristine", "Don't check data files during update")
      + pep::commandline::Parameter("update-pseudonym-format", "Renames directories in your download directory from using long pseudonyms to the shorter participant alias")
      + pep::commandline::Parameter("all-accessible", "Download all data to which the current UserGroup has access")
      + MultiCellQuery::Parameters()
      + pep::commandline::Parameter("report-progress", "Produce progress status messages")
      + pep::commandline::Parameter("suppress-file-extensions", "Don't apply file extensions to downloaded files");
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
          .op(pep::RxBeforeCompletion([ctx]() {
            cleanUp(ctx);
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
