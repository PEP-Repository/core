#include <pep/cli/export/CommandExportJson.hpp>

#include <pep/cli/Export.hpp>
#include <pep/structuredoutput/Json.hpp>
#include <pep/application/CommandLineParameter.hpp> // for commandline::Parameters
#include <pep/cli/Command.hpp>
#include <pep/structuredoutput/Table.hpp>

#include <ostream>

namespace {

namespace json = pep::structuredOutput::json;
using namespace pep::cli;

} // namespace

pep::commandline::Parameters CommandExport::CommandExportJson::getSupportedParameters() const {
  using namespace pep::commandline;
  return ChildCommandOf<CommandExport>::getSupportedParameters();
}

void CommandExport::CommandExportJson::writeOutput(
    const pep::structuredOutput::Table& table,
    std::ostream& stream) const {
  json::append(stream, table);
}
