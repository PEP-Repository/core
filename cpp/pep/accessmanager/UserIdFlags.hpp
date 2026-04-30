#pragma once

#include <pep/utils/Attributes.hpp>
#include <pep/utils/TypeTraits.hpp>

namespace pep {

enum class PEP_ATTRIBUTE_FLAG_ENUM UserIdFlags {
  None = 0,
  IsPrimaryId = 0b01,
  IsDisplayId = 0b10,
  All = 0b11,
};

PEP_MARK_AS_FLAG_ENUM_TYPE(UserIdFlags)

}
