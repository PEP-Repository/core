#pragma once

#include <functional>
#include <string>

// Forward declaration
namespace emscripten { class val; }

namespace pep::weblib::tests {

/// Block on Promise<string>.
/// Do not call concurrently
std::string PromiseTest(std::function<emscripten::val()> createPromise);

}
