#pragma once
#include <optional>
#include <string>

namespace pep {
class ThreadName {
public:
  ThreadName() = delete;
  static const std::optional<std::string> &Get();
  static void Set(const std::string &name);

private:
  static thread_local std::optional<std::string> mName;
};
}