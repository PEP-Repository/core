#include <pep/cli/ColumnQuery.hpp>

using namespace pep::cli;

pep::commandline::Parameters ColumnQuery::Parameters() {
  return pep::commandline::Parameters()
    + pep::commandline::Parameter("column", "Columns to include").alias("columns").shorthand('c').value(pep::commandline::Value<std::string>().multiple())
    + pep::commandline::Parameter("column-group", "Column groups to include").alias("column-groups").shorthand('C').value(pep::commandline::Value<std::string>().multiple());
}

std::vector<std::string> ColumnQuery::GetColumnGroups(const pep::commandline::NamedValues& values) {
  return values.getOptionalMultiple<std::string>("column-group");
}

std::vector<std::string> ColumnQuery::GetColumns(const pep::commandline::NamedValues& values) {
  return values.getOptionalMultiple<std::string>("column");
}
