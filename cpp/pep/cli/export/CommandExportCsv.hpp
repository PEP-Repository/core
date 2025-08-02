#pragma once

#include <ostream>
#include <pep/cli/Export.hpp>
#include <pep/cli/Command.hpp>
#include <string_view>

/// CLI command to convert pulled data to CSV
class pep::cli::CommandExport::CommandExportCsv: public CommandExport::ChildCommand {
public:
  explicit CommandExportCsv(CommandExport& parent)
    : ChildCommand("csv", "create a csv summary of pepcli pull results", parent) {}

protected:
  std::string_view preferredExtension() const noexcept override { return ".csv"; }
  void writeOutput(const pep::structuredOutput::Table&, std::ostream&) const override;
  pep::commandline::Parameters getSupportedParameters() const override;
};
