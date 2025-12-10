#pragma once

#include <ostream>
#include <pep/application/Application.hpp>
#include <pep/cli/Command.hpp>
#include <pep/cli/structuredoutput/TableFromDownloadDirectory.hpp>
#include <pep/structuredoutput/Table.hpp>
#include <string_view>

namespace pep::cli {

/// Commands to convert pepcli results into other formats
class CommandExport : public ChildCommandOf<CliApplication> {
public:
  explicit CommandExport(CliApplication& parent)
    : ChildCommandOf<CliApplication>("export", "Convert pull results to other formats", parent) {}

protected:
  using ConversionConfig = pep::structuredOutput::TableFromDownloadDirectoryConfig;

  /// Specialization of the generic ChildCommandOf<T> for CommandExport,
  /// contains all functionality that is shared by all child commands of export
  class ChildCommand : public ChildCommandOf<CommandExport> {
  protected:
    using ChildCommandOf<CommandExport>::ChildCommandOf; // 'inherit' parent constructors

    enum class AllowOverwrite { no, yes };

    /// Configuration options that are shared by all formats (child commands)
    struct CommonConfiguration final {
      std::filesystem::path inputDirectory; ///< absolute path to the input directory
      std::filesystem::path outputFile;     ///< absolute path, where the output will be written to
      AllowOverwrite allowOverwrite;        ///< Whether or not it is allowed to write the output over an existing file
      ConversionConfig conversion; ///< How the DownloadDirectory to a table
    };

    CommonConfiguration commonConfiguration(ConversionConfig::IdTextFunction) const;
    void abortIfNotWritable(const std::filesystem::path&, AllowOverwrite) const;
    void safeWriteOutput(const pep::structuredOutput::Table&, const std::filesystem::path&, AllowOverwrite) const;

    int execute() final;

    /// format specific file extension (including '.')
    virtual std::string_view preferredExtension() const noexcept = 0;
    virtual void writeOutput(const pep::structuredOutput::Table&, std::ostream&) const = 0;
  };

  std::vector<std::shared_ptr<pep::commandline::Command>> createChildCommands() override;
  pep::commandline::Parameters getSupportedParameters() const override;

private:
  friend class ChildCommand; // Allows a ChildCommand to make calls like 'this->getParent().getSupportedParameters()'
  class CommandExportCsv;
  class CommandExportJson;
  class CommandExportYaml;
};

}
