#pragma once

#include <nlohmann/json.hpp>
#include <pep/structuredoutput/Table.hpp>

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
  static Tree FromJson(nlohmann::json json) noexcept { return Tree(std::move(json)); }

  const nlohmann::json& toJson() const& noexcept { return mJson; }
  nlohmann::json toJson() && noexcept { return std::move(mJson); }

private:
  explicit Tree(nlohmann::json json) noexcept : mJson(std::move(json)) {}
  nlohmann::json mJson;
};

/// Converts a Table to a Tree
Tree TreeFromTable(const Table&);

} // namespace pep::structuredOutput
