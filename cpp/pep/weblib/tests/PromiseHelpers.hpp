#pragma once

#include <pep/weblib/Emscripten_fwd.hpp>

#include <functional>
#include <string>

namespace pep::weblib::tests {

/// Block on Promise<string>.
/// Do not call concurrently
std::string PromiseTest(std::function<emscripten::val()> createPromise);

}
