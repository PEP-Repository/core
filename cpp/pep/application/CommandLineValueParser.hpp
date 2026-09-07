#pragma once

#include <string>
#include <type_traits>
#include <pep/utils/ChronoUtil.hpp>
#include <pep/utils/Timestamp.hpp>

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
struct ValueParser<T, std::enable_if_t<chrono::IsDuration<T>::value>> {
  T operator()(const std::string& argument) { return chrono::ParseDuration<T>(argument); }
};

template <typename T>
struct ValueParser<T, std::enable_if_t<std::is_same_v<T, Timestamp>>> {
  T operator()(const std::string& argument) {
    constexpr std::string_view unixtimePrefix{"unix:"};
    constexpr std::string_view unixtimeMsPrefix{"unix-ms:"};
    if (argument.starts_with(unixtimePrefix)) {
      auto timestampString = std::string_view(argument).substr(unixtimePrefix.length());
      return Timestamp(std::chrono::seconds{boost::lexical_cast<Timestamp::rep>(timestampString)});
    }
    if (argument.starts_with(unixtimeMsPrefix)) {
      auto timestampString = std::string_view(argument).substr(unixtimeMsPrefix.length());
      return Timestamp(std::chrono::milliseconds{boost::lexical_cast<Timestamp::rep>(timestampString)});
    }
    if (argument.find("-") != std::string::npos) {
      return TimeZone::Local().timestampFromXmlDateTime(argument);
    }
    return TimeZone::Local().timestampFromYyyyMmDd(argument);
  }
};

}
}
