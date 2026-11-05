#pragma once

#include <exception>
#include <string>
#include <utility>

class SerializeException : public std::exception {
 public:
  SerializeException() = default;
  SerializeException(std::string what) : wh_(std::move(what)) {}

  const char* what() const noexcept override {
    return wh_.c_str();
  }

 private:
  std::string wh_;
};
