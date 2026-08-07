#pragma once

#include <pep/application/CommandLineParameter.hpp>

namespace pep::cli {

struct ColumnQuery {
  static pep::commandline::Parameters Parameters();

  static std::vector<std::string> GetColumnGroups(const pep::commandline::NamedValues& values);
  static std::vector<std::string> GetColumns(const pep::commandline::NamedValues& values);
};

}
