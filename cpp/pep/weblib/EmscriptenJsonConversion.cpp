#include <pep/weblib/EmscriptenJsonConversion.hpp>

#include <pep/utils/Log.hpp>

#include <emscripten/val.h>
#include <nlohmann/json.hpp>

#include <concepts>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

using namespace pep;

namespace {
const std::string LOG_TAG = "EmscriptenJsonConversion";

/// Does this number fit in the integer portion of a float?
template <typename Float>
constexpr bool CanRepresentAsFloat(std::integral auto num) {
  static_assert(std::numeric_limits<Float>::radix == 2, "Only binary floats are supported");
  constexpr auto MantissaBits = std::numeric_limits<double>::digits;
  constexpr auto MaxRepresentable = std::int64_t{1} << MantissaBits;
  return std::cmp_greater_equal(num, -MaxRepresentable)
         && std::cmp_less_equal(num, MaxRepresentable);
}
}

nlohmann::json pep::weblib::EmValToJson(const emscripten::val& v) {
  using emscripten::val;
  using nlohmann::json;

  const auto type = v.typeOf().as<std::string>();
  if (type == "undefined") {
    LOG(LOG_TAG, debug) << "Discarding `undefined`";
    return json::value_t::discarded;
  }
  else if (type == "null") return nullptr;
  else if (type == "boolean") return v.as<bool>();
  else if (type == "number") return v.as<double>();
  else if (type == "string") return v.as<std::string>();
  else if (type == "bigint") {
    if (v < val{0}) {
      if (v < val{std::numeric_limits<json::number_integer_t>::min()}) {
        LOG(LOG_TAG, debug) << "Discarding too small bigint";
        return json::value_t::discarded;
      }
      return v.as<json::number_integer_t>();
    }
    if (v > val{std::numeric_limits<json::number_unsigned_t>::max()}) {
      LOG(LOG_TAG, debug) << "Discarding too large bigint";
      return json::value_t::discarded;
    }
    return v.as<json::number_unsigned_t>();
  }
  else if (type == "object") {
    if (v.isArray()) {
      json arr;
      for (const val& elem : v) {
        arr.push_back(EmValToJson(elem));
      }
      return arr;
    } else {
      json obj;
      for (const auto& entry : val::global("Object").call<val>("entries", v)) {
        obj[entry[0].as<std::string>()] = EmValToJson(entry[1]);
      }
      return obj;
    }
  }
  else {
    // Unsupported type, e.g. function / symbol
    LOG(LOG_TAG, debug) << "Discarding value of type " << type;
    return json::value_t::discarded;
  }
}

emscripten::val pep::weblib::EmValFromJson(const nlohmann::json& v) {
  using nlohmann::json;
  using enum json::value_t;
  using emscripten::val;
  switch (v.type()) {
    case null:
      return val::null();
    case object: {
      val obj = val::object();
      for (const auto &[key, value]: v.items()) {
        obj.set(key, EmValFromJson(value));
      }
      return obj;
    }
    case array: {
      val obj = val::array();
      for (const auto &value: v) {
        obj.call<void>("push", EmValFromJson(value));
      }
      return obj;
    }
    case string:
      return val(v.get<std::string>());
    case boolean:
      return val(v.get<bool>());
    case number_integer: {
      const auto num = v.get<json::number_integer_t>();
      return CanRepresentAsFloat<double>(num)
             ? val(static_cast<double>(num))
             : val(num);
    }
    case number_unsigned: {
      const auto num = v.get<json::number_unsigned_t>();
      return CanRepresentAsFloat<double>(num)
             ? val(static_cast<double>(num))
             : val(num);
    }
    case number_float:
      return val(v.get<double>());
    case binary:
      throw std::runtime_error("Binary JSON values non implemented");
    case discarded:
      return val::undefined();
  }
}
