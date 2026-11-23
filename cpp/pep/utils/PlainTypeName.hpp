#pragma once

#include <boost/type_index.hpp>

#include <string>

namespace pep {

namespace detail {

std::string BoostPrettyTypeNameToPlain(std::string name);

}

template <typename T>
std::string GetPlainTypeName() {
  return detail::BoostPrettyTypeNameToPlain(boost::typeindex::type_id<T>().pretty_name());
}

}
