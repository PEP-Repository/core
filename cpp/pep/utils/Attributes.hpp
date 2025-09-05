#pragma once

/// @def PEP_ATTRIBUTE_FLAG_ENUM
/// @brief Defines a compiler specific attribute to mark a enum type as a flag type.
///
/// Normally the compiler assumes that the only valid enum values are those that are explicitly included in
/// its definition. This is not true for flag types, where any bitwise combination of enum values is valid, even if it
/// was not expliticly defined.

#if defined(__clang__)
  #define PEP_ATTRIBUTE_FLAG_ENUM [[clang::flag_enum]]
#else // no compiler support
  #define PEP_ATTRIBUTE_FLAG_ENUM
#endif

