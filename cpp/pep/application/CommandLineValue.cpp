#include <pep/application/CommandLineValue.hpp>

namespace pep {
namespace commandline {

size_t NamedValues::count(const std::string& key) const noexcept {
  if (this->has(key)) {
    return mEntries.at(key).count();
  }
  return 0U;
}

bool NamedValues::hasAnyOf(std::initializer_list<std::string> keys) const noexcept {
  for (const auto &key : keys) {
    if (this->has(key)) {
      return true;
    }
  }
  return false;
}

}
}
