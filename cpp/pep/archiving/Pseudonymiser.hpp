#pragma once

#include <functional>
#include <string>
#include <iostream>

namespace pep {
class Pseudonymiser {
public:
  explicit Pseudonymiser(const std::string& oldValue, const std::string& newValue = "") : oldValue_(oldValue), newValue_(newValue) {
    if (newValue.empty()) {
      newValue_ = GetDefaultPlaceholder().substr(0, oldValue.length());
    }
  }
  void pseudonymise(std::istream& in, std::function<void(const char*, const std::streamsize)> writeToDestination);

  static const std::string& GetDefaultPlaceholder();

private:
  std::string oldValue_;
  std::string newValue_;
};
}
