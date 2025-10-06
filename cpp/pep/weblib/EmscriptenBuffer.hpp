#pragma once

#include <string>

// Forward declaration
namespace emscripten { class val; }

namespace pep::weblib {

class Buffer {
public:
  std::string data;
  [[nodiscard]] emscripten::val view() const;
};

}
