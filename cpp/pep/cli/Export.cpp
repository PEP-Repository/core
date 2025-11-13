#include <cstddef>
#include <filesystem>
#include <fstream>
#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/cli/Export.hpp>
#include <pep/cli/export/CommandExportCsv.hpp>
#include <pep/cli/export/CommandExportJson.hpp>
#include <pep/core-client/CoreClient.hpp>
#include <pep/structuredoutput/Table.hpp>
#include <pep/utils/Filesystem.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <stdexcept>
#include <string_view>

using namespace pep::cli;

namespace {

using PathStyle = pep::structuredOutput::TableFromDownloadDirectoryConfig::PathStyle;

std::filesystem::path DefaultToExtension(std::filesystem::path p, std::string_view defaultExtension) {
  if (!p.has_extension()) { p.replace_extension(defaultExtension); }
  return p;
}

PathStyle::Variant DeterminePathStyle(
    std::string_view style,
    const std::filesystem::path& input,
    const std::filesystem::path& output) {
  if (style == "uri") return PathStyle::FileUri{};
  if (style == "absolute") return PathStyle::Absolute{};
  if (style == "relative-to-output") return PathStyle::RelativeTo{output};
  if (style == "relative-to-input") return PathStyle::RelativeTo{input};
  throw std::logic_error{"No logic to handle path style choice \"" + std::string{style} + "\""};
}

} // namespace

void CommandExport::ChildCommand::abortIfNotWritable(
    const std::filesystem::path& where,
    AllowOverwrite allowOverwrite) const {
  const auto isOverwritable = (allowOverwrite == AllowOverwrite::yes) && is_regular_file(where);
  if (exists(where) && !isOverwritable) {
    throw std::runtime_error{
        "Cannot write over \"" + where.string()
        + "\". Please specify another output location or use '--force' to overwrite the existing file."};
  }
}

void CommandExport::ChildCommand::safeWriteOutput(
    const pep::structuredOutput::Table& table,
    const std::filesystem::path& output,
    AllowOverwrite allowOverwrite) const {
  namespace fs = pep::filesystem;
  auto tempFile = fs::Temporary{output.string() + fs::RandomizedName(".%%%%%%%%.tmp")};
  {
    std::ofstream stream{tempFile.path()};
    writeOutput(table, stream);
  } // flush and delete stream
  abortIfNotWritable(output, allowOverwrite); // late check, just before writing (filesystem could have changed)
  rename(tempFile.path(), output);
}

std::vector<std::shared_ptr<pep::commandline::Command>> CommandExport::createChildCommands() {
  return {std::make_shared<CommandExportCsv>(*this), std::make_shared<CommandExportJson>(*this)};
}

pep::commandline::Parameters CommandExport::getSupportedParameters() const {
  using namespace pep::commandline;
  const auto conversionDefaults = pep::structuredOutput::TableFromDownloadDirectoryConfig{};
  return ChildCommandOf<CliApplication>::getSupportedParameters()
      + Parameter("from", "Directory with pull results to use as input (relative to current working directory)")
            .value(Value<std::filesystem::path>().directory().defaultsTo("pulled-data"))
      + Parameter("output-file", "File to write the export results to (relative to current working directory)")
            .shorthand('o')
            .value(Value<std::filesystem::path>().defaultsTo("export"))
      + Parameter("no-auto-extension", "Disables automatic addition of an output-file extension")
      + Parameter("force", "Overwrite existing files").shorthand('f')
      + Parameter("max-inline-size", "Files larger than this many bytes are not inlined")
            .value(Value<std::size_t>().defaultsTo(conversionDefaults.maxInlineSizeInBytes))
      + Parameter("file-reference-style", "How paths to external files are represented in the output")
            .value(Value<std::string>()
                       .allow(std::array{"uri", "absolute", "relative-to-output", "relative-to-input"})
                       .defaultsTo("relative-to-input"))
      + Parameter("file-reference-postfix", "Columns containing references to external files get this postfix")
            .value(Value<std::string>().defaultsTo(conversionDefaults.fileReferencePostfix));
}

int CommandExport::ChildCommand::execute() {
  return executeEventLoopFor(false, [this](std::shared_ptr<pep::CoreClient> client) {
    return client->getGlobalConfiguration().map([this](std::shared_ptr<pep::GlobalConfiguration> globalConfig) {
      const auto idToText = [globalConfig](const ParticipantIdentifier& id) {
        return globalConfig->getUserPseudonymFormat().makeUserPseudonym(id.getLocalPseudonym());
      };
      const auto config = commonConfiguration(idToText);
      const auto existingDownloadDir = DownloadDirectory::Create(config.inputDirectory, globalConfig, [](std::shared_ptr<const Progress>) {}); // TODO: report (instead of ignore) progress
      abortIfNotWritable(config.outputFile, config.allowOverwrite); // early check, before doing any work
      const auto table = pep::structuredOutput::TableFrom(*existingDownloadDir, config.conversion);
      safeWriteOutput(table, config.outputFile, config.allowOverwrite);
      std::cout << config.outputFile.string() << std::endl; // explicit conversion to string to get unquoted output
      return pep::FakeVoid{};
    });
  });
}

CommandExport::ChildCommand::CommonConfiguration CommandExport::ChildCommand::commonConfiguration(
    ConversionConfig::IdTextFunction idTextFunction) const {
  const auto& values = getParent().getParameterValues();
  auto config = CommonConfiguration{};
  config.inputDirectory = absolute(values.get<std::filesystem::path>("from"));
  config.outputFile = absolute(values.get<std::filesystem::path>("output-file"));
  config.outputFile =
      DefaultToExtension(config.outputFile, values.has("no-auto-extension") ? "" : preferredExtension());
  config.allowOverwrite = values.has("force") ? AllowOverwrite::yes : AllowOverwrite::no;
  config.conversion.maxInlineSizeInBytes = values.get<std::size_t>("max-inline-size");
  config.conversion.pathStyle =
      DeterminePathStyle(values.get<std::string>("file-reference-style"), config.inputDirectory, config.outputFile);
  config.conversion.fileReferencePostfix = values.get<std::string>("file-reference-postfix");
  config.conversion.idText = std::move(idTextFunction);
  return config;
}

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandExport(CliApplication& parent) {
  return std::make_shared<CommandExport>(parent);
}
