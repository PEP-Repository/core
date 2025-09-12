#include <pep/cli/export/CommandExportCsv.hpp>

#include <pep/cli/Export.hpp>
#include <pep/structuredoutput/Csv.hpp>
#include <string_view>

using namespace pep::cli;

namespace {

namespace csv = pep::structuredOutput::csv;

csv::Delimiter CsvDelimiter(std::string_view str) {
  if (str == "comma") { return csv::Delimiter::comma; }
  if (str == "semicolon") { return csv::Delimiter::semicolon; }
  if (str == "tab") { return csv::Delimiter::tab; }
  throw std::logic_error{"No logic to handle csv delimiter choice \"" + std::string{str} + "\""};
}

} // namespace

pep::commandline::Parameters CommandExport::CommandExportCsv::getSupportedParameters() const {
  using namespace pep::commandline;
  return ChildCommandOf<CommandExport>::getSupportedParameters()
      + Parameter("delimiter", "delimiter used to separate fields in the CSV file")
            .value(Value<std::string>().allow(std::array{"comma", "semicolon", "tab"}).defaultsTo("comma"));
}

void CommandExport::CommandExportCsv::writeOutput(
    const pep::structuredOutput::Table& table,
    std::ostream& stream) const {
  const auto& values = getParameterValues();
  csv::append(stream, table, {.delimiter = CsvDelimiter(values.get<std::string>("delimiter"))});
}
