#pragma once

#include <cstdint>
#include <pep/utils/Attributes.hpp>
#include <pep/utils/TypeTraits.hpp>

namespace pep {

enum class PEP_ATTRIBUTE_FLAG_ENUM UserIdFlags : uint8_t {
  None = 0,
  IsPrimaryId = 1 << 0,
  IsDisplayId = 1 << 1,
  All = (1 << 2) - 1,
};

PEP_MARK_AS_FLAG_ENUM_TYPE(UserIdFlags)

}
