#pragma once
#include <optional>
#include <string>

namespace pep {
struct ThreadName {
  ThreadName() = delete;
  static const std::optional<std::string> &Get();
  static void Set(const std::string &name);
};
}
