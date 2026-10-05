#pragma once

#include <nlohmann/json.hpp>
#include <pep/structuredoutput/Table.hpp>
#include <pep/structuredoutput/Common.hpp>
#include <pep/accessmanager/UserMessages.hpp>
#include <pep/accessmanager/AmaMessages.hpp>

#include <boost/property_tree/ptree_fwd.hpp>

namespace pep::structuredOutput {

/// Generic tree structure that converts to other (more specific) formats
///
/// This is a purely semantic class to signal that we only care
/// about the structure of the tree and not the underlying format.
///
/// Internally represented as JSON and therefore supports conversion to and from that format by default.
/// Other conversions should be provided as external functions.
class Tree final {
public:
  static Tree FromJson(nlohmann::ordered_json json) noexcept { return Tree(std::move(json)); }
  static Tree FromPropertyTree(const boost::property_tree::ptree&);

  const nlohmann::ordered_json& rawJson() const& noexcept { return json_; }
  nlohmann::ordered_json rawJson() && noexcept { return std::move(json_); }

private:
  explicit Tree(nlohmann::ordered_json json) noexcept : json_(std::move(json)) {}
  nlohmann::ordered_json json_;
};

/// Converts a Table to a Tree
Tree TreeFromTable(const Table&);

} // namespace pep::structuredOutput
