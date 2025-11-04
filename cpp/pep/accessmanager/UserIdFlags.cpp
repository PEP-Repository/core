#include <pep/accessmanager/UserIdFlags.hpp>

#include <pep/utils/TypeTraits.hpp>

namespace pep {
UserIdFlags operator|(UserIdFlags lhs, UserIdFlags rhs)
{
    return static_cast<UserIdFlags>(ToUnderlying(lhs) | ToUnderlying(rhs));
}

UserIdFlags operator&(UserIdFlags lhs, UserIdFlags rhs)
{
    return static_cast<UserIdFlags>(ToUnderlying(lhs) & ToUnderlying(rhs));
}

UserIdFlags operator|=(UserIdFlags& lhs, UserIdFlags rhs) {
  lhs = lhs | rhs;
  return lhs;
}
}