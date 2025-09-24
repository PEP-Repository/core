#include <pep/structuredoutput/Tree.hpp>

namespace pep::structuredOutput {
namespace {

using nlohmann::json;
using ConstRecordRef = Table::ConstRecordRef;
using OptConstRecordRef = std::optional<ConstRecordRef>;

json AsArray(ConstRecordRef record) {
  auto array = json::array();
  for (const auto& field : record) { array += field; }
  return array;
}

json ObjectFromHeaderAndRecord(ConstRecordRef header, ConstRecordRef record) {
  assert(header.size() == record.size());

  auto object = json::object();
  for (std::size_t i = 0; i < record.size(); ++i) { object.emplace(header[i], record[i]); }
  return object;
}

/// Converts the table to an array of objects (AoO)
json JsonArray(const Table& table) {
  auto array = json::array();
  for (auto record : table.records()) { array += ObjectFromHeaderAndRecord(table.header(), record); }
  return array;
}

} // namespace

Tree TreeFromTable(const Table& table) {
  auto data = JsonArray(table);
  auto metadata = json::object({{"header", AsArray(table.header())}});
  return Tree::FromJson(json::object({{"metadata", std::move(metadata)}, {"data", std::move(data)}}));
}

} // namespace pep::structuredOutput
