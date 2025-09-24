#pragma once

#include <string>
#include <type_traits>
#include <pep/utils/ChronoUtil.hpp>

#include <boost/lexical_cast.hpp>

namespace pep {
namespace commandline {

template <typename T, typename = void>
struct ValueParser {
  T operator()(const std::string& argument) { return T(argument); }
};

template <typename T>
struct ValueParser<T, std::enable_if_t<std::is_integral_v<T>>> {
  T operator()(const std::string& argument) { return boost::lexical_cast<T>(argument); }
};

template <typename T>
struct ValueParser<T, std::enable_if_t<chrono::is_duration<T>::value>> {
  T operator()(const std::string& argument) { return chrono::ParseDuration<T>(argument); }
};

}
}
