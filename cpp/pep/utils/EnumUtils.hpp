#pragma once

#include <pep/utils/TypeTraits.hpp>

namespace pep {

//XXX Replace by std::to_underlying in C++23
[[nodiscard]] constexpr auto ToUnderlying(Enum auto v) { return static_cast<std::underlying_type_t<decltype(v)>>(v); }

}
