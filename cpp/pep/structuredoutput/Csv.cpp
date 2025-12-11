#include <cassert>
#include <ostream>
#include <pep/structuredoutput/Csv.hpp>
#include <pep/structuredoutput/Table.hpp>
#include <sstream>

namespace pep::structuredOutput::csv {

namespace {

struct StringLiteral final {
  const std::string& strRef;
};

/// encloses the string in double-quotes and escapes characters if necessary
std::ostream& operator<< (std::ostream& lhs, const StringLiteral rhs) {
  lhs << '"';
  for (const char& c : rhs.strRef) {
    // escape double-quote characters by duplication
    if (c == '"') { lhs << c << c; }
    else { lhs << c; }
  }
  return lhs << '"';
}

std::ostream& append(std::ostream& stream, Table::ConstRecordRef record, Config config) {
  assert(record.size() >= 1); // The implementation of this function uses this class invariant of Table

  const auto delimiter = static_cast<char>(config.delimiter);
  for (const auto& field : record) {
    const auto isLast = std::addressof(field) == std::addressof(record.back());
    stream << StringLiteral{field} << (isLast ? '\n' : delimiter);
  }
  return stream;
}

} // namespace

std::ostream& append(std::ostream& stream, const Table& table, Config config) {
  append(stream, table.header(), config);
  for (const auto& record : table.records()) { append(stream, record, config); }
  return stream;
}

std::string to_string(const Table& table, Config config) {
  std::ostringstream stream;
  append(stream, table, config);
  return std::move(stream).str();
}

} // namespace pep::structuredOutput::csv
