#pragma once

#include <pep/application/CommandLineParameter.hpp> // for commandline::Parameters
#include <pep/cli/Export.hpp>
#include <pep/structuredoutput/Table.hpp>

#include <ostream>
#include <string_view>

/// CLI command to convert pulled data to JSON
class pep::cli::CommandExport::CommandExportJson: public CommandExport::ChildCommand {
public:
  explicit CommandExportJson(CommandExport& parent)
    : ChildCommand("json", "create a json summary of pepcli pull results", parent) {}

protected:
  std::string_view preferredExtension() const noexcept override { return ".json"; }
  void writeOutput(const pep::structuredOutput::Table&, std::ostream&) const override;
  pep::commandline::Parameters getSupportedParameters() const override;
};
