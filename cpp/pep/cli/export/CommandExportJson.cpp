#include <pep/cli/export/CommandExportJson.hpp>

#include <pep/cli/Export.hpp>
#include <pep/structuredoutput/Json.hpp>
#include <string_view>

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
