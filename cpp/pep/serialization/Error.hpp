#pragma once

#include <exception>
#include <string>

namespace pep {

class Error : public std::exception {
public:
  explicit inline Error(std::string description)
    : description_(std::move(description)) { }

  std::string description_;

  inline const char* what() const noexcept override { return description_.c_str(); }

  static bool IsSerializable(std::exception_ptr exception) noexcept;
  static std::exception_ptr ReconstructIfDeserializable(std::string_view serialized);
  static void ThrowIfDeserializable(std::string_view serialized);
};

}
