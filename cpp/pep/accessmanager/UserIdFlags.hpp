#pragma once

#include <stdint.h>
#include <pep/utils/Attributes.hpp>

namespace pep {

enum class PEP_ATTRIBUTE_FLAG_ENUM UserIdFlags : uint8_t {
  none = 0,
  isPrimaryId = 1<<0,
  isDisplayId = 1<<1,
};

UserIdFlags operator|(UserIdFlags lhs, UserIdFlags rhs);
UserIdFlags operator&(UserIdFlags lhs, UserIdFlags rhs);
UserIdFlags operator|=(UserIdFlags& lhs, UserIdFlags rhs);

}