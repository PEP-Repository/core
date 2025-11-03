#pragma once

#include <stdint.h>

namespace pep {

enum class UserIdFlags : uint8_t {
  none = 0,
  isPrimaryId = 1,
  isDisplayId = 2,
};

UserIdFlags operator|(UserIdFlags lhs, UserIdFlags rhs);
UserIdFlags operator&(UserIdFlags lhs, UserIdFlags rhs);
UserIdFlags operator|=(UserIdFlags& lhs, UserIdFlags rhs);

}