#pragma once

#include <pep/utils/Attributes.hpp>
#include <pep/utils/EnumUtils.hpp>

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pep {

/// Access modes for column groups
enum class PEP_ATTRIBUTE_FLAG_ENUM ColumnAccessModes {
  None = 0b0'0000,
  Read = 0b0'0001,
  ReadMeta = 0b0'0010,
  Write = 0b0'0100,
  WriteMeta = 0b0'1000,
  Enumerate = 0b1'0000,
  All = Read | ReadMeta | Write | WriteMeta | Enumerate,
};
}

PEP_MARK_AS_FLAG_ENUM_TYPE(::pep::ColumnAccessModes)

namespace pep {

/// Returns \p modes extended with the modes that would be granted implicitly
///
/// Read grants ReadMeta and WriteMeta grants Write.
constexpr ColumnAccessModes EffectiveModes(const ColumnAccessModes modes) noexcept {
  return modes
    | FlagsIf(ColumnAccessModes::ReadMeta, HasFlags(modes, ColumnAccessModes::Read))
    | FlagsIf(ColumnAccessModes::Write, HasFlags(modes, ColumnAccessModes::WriteMeta));
}

/// True if the (implied) \p granted modes cover the \p required modes
constexpr bool ImpliesModes(const ColumnAccessModes granted, const ColumnAccessModes required) noexcept {
  return HasFlags(EffectiveModes(granted), required);
}

/// Names of the individual modes that are set in \p modes, in declaration order.
std::vector<std::string> ToIndividualStrings(ColumnAccessModes modes);

/// Names of all modes that are set in \p modes, joined into a single string.
std::string ToSingleString(ColumnAccessModes modes, std::string_view separator = "|");

/// Interpret \p name (like "read") as an individual mode.
/// \throws Error if \p name doesn't denote a known mode.
ColumnAccessModes ParseMode(std::string_view name);

/// Interpret \p names as a combination of modes.
/// \throws Error if one of \p names doesn't denote a known mode.
ColumnAccessModes ParseModes(const std::span<const std::string> names);

} // namespace pep
