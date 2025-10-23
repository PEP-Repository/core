#pragma once

#include <string>
#include <boost/type_index.hpp>

namespace pep {

namespace detail {

std::string BoostPrettyTypeNameToPlain(std::string name);

}

template <typename T>
std::string GetPlainTypeName() {
  return detail::BoostPrettyTypeNameToPlain(boost::typeindex::type_id<T>().pretty_name());
}

}
