#pragma once

#include <pep/utils/NormalizedTypeNaming.hpp>

#include <ostream>
#include <ranges>
#include <string>
#include <variant>
#include <vector>

namespace pep {

struct PrefixedString {
  std::string string;
};

struct PrefixedStrings {
  std::vector<std::string> strings;
};

struct PushPrefix {
  std::string prefix;
};

struct PopPrefix {};

using PrefixedStringsEntry = std::variant<PrefixedString, PrefixedStrings, PushPrefix, PopPrefix>;

// Name the variant after its protobuf message, so that (de)serialization can find its message magic
template <>
struct NormalizedTypeNamer<PrefixedStringsEntry> : BasicNormalizedTypeNamer {
  static std::string GetTypeName() { return "PrefixedStringsEntry"; }
};

namespace detail {
class PrefixedStringWriter {
  std::ostream& out_;
  char separator_;
  std::vector<std::string> prefixes_;

  void ensureNoSeparator(std::string_view str, const char* errorMessage) const;
  void output(std::string_view str) const;

public:
  PrefixedStringWriter(std::ostream& out, char separator) : out_(out), separator_(separator) {}

  void operator()(const PrefixedString& prefixedString) const;
  void operator()(const PrefixedStrings& prefixedStrings) const;
  void operator()(PushPrefix pushPrefix);
  void operator()(PopPrefix);

  /// \brief Ensures that all pushed prefixes have been popped.
  void finish() const;
};
}

/// \brief Writes prefixed strings (such as page paths) to a stream: each string is preceded by its (concatenated)
///        prefixes and followed by a separator.
/// \param out The stream to write to.
/// \param strings The entries to write. Every PushPrefix must be matched by a (later) PopPrefix.
/// \param separator The character to write after each string. Prefixes and strings may not contain it, so that the
///                  output can be split unambiguously.
/// \remark Throws std::runtime_error if a prefix or string contains the separator, or if the PushPrefix and PopPrefix
///         entries don't match up. Output that precedes the problem will have been written by then.
void WritePrefixedStrings(std::ostream& out, std::ranges::input_range auto&& strings, char separator = '\n')
requires(std::same_as<std::ranges::range_value_t<decltype(strings)>, PrefixedStringsEntry>) {
  detail::PrefixedStringWriter writer(out, separator);
  for (auto&& entry : std::forward<decltype(strings)>(strings)) {
    std::visit(writer, std::forward<decltype(entry)>(entry));
  }
  writer.finish();
}

}
