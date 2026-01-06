#pragma once

#include <nlohmann/json_fwd.hpp>

// Forward declaration
namespace emscripten { class val; }

namespace pep::weblib {
//TODO implement nlohmann::from_json/nlohmann::to_json instead
nlohmann::json EmValToJson(const emscripten::val& val);
emscripten::val EmValFromJson(const nlohmann::json& val);
}
