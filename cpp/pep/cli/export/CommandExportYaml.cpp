#include <pep/cli/export/CommandExportYaml.hpp>

#include <pep/cli/Export.hpp>
#include <pep/structuredoutput/Yaml.hpp>
#include <pep/structuredoutput/Table.hpp>

#include <ostream>

using namespace pep::cli;

void CommandExport::CommandExportYaml::writeOutput(
    const pep::structuredOutput::Table& table,
    std::ostream& stream) const {
  pep::structuredOutput::yaml::append(stream, table);
}
