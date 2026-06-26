#pragma once
#include <exception>

namespace pep {

/// Exception class usable in tests
struct TestError : std::exception {
  [[nodiscard]] const char* what() const noexcept override { return "TestError"; }
};

}
