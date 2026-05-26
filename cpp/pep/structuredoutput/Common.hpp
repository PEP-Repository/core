#pragma once

#include <string>
#include <string_view>
#include <variant>
#include <cstddef>

#include <pep/utils/VariantUtils.hpp>

namespace pep::structuredOutput {

enum class WhitespaceFormat {
  Compact,   // No indentation (single line)
  TwoSpaces, // 2 spaces per indentation level
  FourSpaces // 4 spaces per indentation level
};

namespace queryKeys {
struct QueryKey final {
  std::string_view simple;
  std::string_view descriptive;
};

// common query keys
constexpr QueryKey name{"name", "Name"};
constexpr QueryKey userGroup{"user-group", "User Group"};
constexpr QueryKey userGroups{"user-groups", "User Groups"}; // input for --include flag

/// Helper to get appropriate key name
inline std::string GetKeyName(const queryKeys::QueryKey& key, bool useDescriptive) {
  return std::string(useDescriptive ? key.descriptive : key.simple);
}

} // namespace queryKeys

enum class Format {
  Yaml,
  Json
};

struct YamlConfig final {
  WhitespaceFormat indentation = WhitespaceFormat::TwoSpaces;
  bool includeArraySizeComments = false; ///< Adds a comment to the header of each non-empty list, displaying the number of elements
  std::size_t arrayCountCommentThreshold = 5; ///< Minimum array size to show item count comment (unless empty)
  bool includeEmptyArrayComments = false; ///< Show item count comment for empty arrays (size 0) when enabled
};

struct JsonConfig final {
  WhitespaceFormat wsformat = WhitespaceFormat::TwoSpaces;
};

using FormatConfig = std::variant<YamlConfig, JsonConfig>;

template<typename FlagsEnum>
struct QueryDisplayConfig final {
  using Flags = FlagsEnum;
  
  Flags flags = Flags::All;
  bool useDescriptiveKeys = true; ///< Controls whether to use descriptive keys ("Display ID") vs simple keys ("display-id") for all fields
  
  FormatConfig formatConfig = YamlConfig{};
  
  Format format() const {
    return std::visit(
      pep::variant_utils::Overloaded{
          [](YamlConfig) { return Format::Yaml; },
          [](JsonConfig) { return Format::Json; }},
      formatConfig);
  }
};

} // namespace pep::structuredOutput