#include <pep/accessmanager/UserIdFlags.hpp>

#include <type_traits>

namespace pep {
UserIdFlags operator|(UserIdFlags lhs, UserIdFlags rhs)
{
    return static_cast<UserIdFlags>(static_cast<std::underlying_type_t<UserIdFlags>>(lhs) |
      static_cast<std::underlying_type_t<UserIdFlags>>(rhs));
}

UserIdFlags operator&(UserIdFlags lhs, UserIdFlags rhs)
{
    return static_cast<UserIdFlags>(static_cast<std::underlying_type_t<UserIdFlags>>(lhs) &
      static_cast<std::underlying_type_t<UserIdFlags>>(rhs));
}

UserIdFlags operator|=(UserIdFlags& lhs, UserIdFlags rhs) {
  lhs = lhs | rhs;
  return lhs;
}
}