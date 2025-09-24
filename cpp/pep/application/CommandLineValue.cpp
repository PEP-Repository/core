#include <pep/application/CommandLineValue.hpp>

namespace pep {
namespace commandline {

void NamedValues::add(const std::string& key, const Values& values) {
  if (!values.empty()) {
    auto& destination = (*this)[key];
    for (const auto& value : values) {
      destination.add(value);
    }
  }
}

size_t NamedValues::count(const std::string& key) const noexcept {
  if (this->has(key)) {
    return mImplementor.at(key).count();
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
