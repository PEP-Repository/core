#pragma once

#include <cstdint>
#include <pep/utils/Attributes.hpp>

namespace pep {

enum class PEP_ATTRIBUTE_FLAG_ENUM UserIdFlags : uint8_t {
  None = 0,
  IsPrimaryId = 1 << 0,
  IsDisplayId = 1 << 1,
  All = (1 << 2) - 1,
};

}
