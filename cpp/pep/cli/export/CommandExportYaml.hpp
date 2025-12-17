#pragma once

#include <pep/cli/Export.hpp>
#include <pep/cli/Command.hpp>

/// CLI command to convert pulled data to YAML
class pep::cli::CommandExport::CommandExportYaml: public CommandExport::ChildCommand {
public:
  explicit CommandExportYaml(CommandExport& parent)
    : ChildCommand("yaml", "create a yaml summary of pepcli pull results", parent) {}

protected:
  std::string_view preferredExtension() const noexcept override { return ".yaml"; }
  void writeOutput(const pep::structuredOutput::Table&, std::ostream&) const override;
};
