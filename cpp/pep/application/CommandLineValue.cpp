#include <pep/application/CommandLineValue.hpp>

#include <algorithm>

namespace pep {
namespace commandline {

size_t NamedValues::count(const std::string& key) const noexcept {
  if (this->has(key)) {
    return entries_.at(key).count();
  }
  return 0U;
}

bool NamedValues::hasAnyOf(std::initializer_list<std::string> keys) const noexcept {
  return std::ranges::any_of(keys, [this](const std::string& key) { return this->has(key); });
}

}
}
