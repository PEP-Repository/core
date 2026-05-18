#include <pep/structuredoutput/Tree.hpp>

#include <boost/property_tree/ptree.hpp>

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

namespace {

json PtreeToJson(const boost::property_tree::ptree& pt) {
  // Check if it's an array or object
  if (pt.empty()) {
    // if its empty its a leaf node
    return json(pt.data());
  }
  
  // if all keys are empty, it's an array
  bool isArray = true;
  for (const auto& [key, value] : pt) {
    if (!key.empty()) {
      isArray = false;
      break;
    }
  }

  if (isArray) {
    json array = json::array();
    for (const auto& [key, value] : pt) {
      array.push_back(PtreeToJson(value));
    }
    return array;
  } else {
    json object = json::object();
    for (const auto& [key, value] : pt) {
      object[key] = PtreeToJson(value);
    }
    return object;
  }
}

} // namespace

Tree Tree::FromPropertyTree(const boost::property_tree::ptree& pt) {
  return Tree::FromJson(PtreeToJson(pt));
}

} // namespace pep::structuredOutput
