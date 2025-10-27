#pragma once

#include <rxcpp/rx-observable-fwd.hpp>

#include <cstddef>
#include <string>

// Forward declaration
namespace emscripten { class val; }

namespace pep {

emscripten::val CreateReadableByteStream(rxcpp::observable<std::string> data, std::size_t chunkSize);

}
