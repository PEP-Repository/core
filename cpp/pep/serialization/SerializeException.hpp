#pragma once

#include <exception>
#include <string>

class SerializeException : public std::exception {
 public:
  SerializeException() = default;
  SerializeException(std::string what) : wh(std::move(what)) {}

  const char* what() const noexcept override {
    return wh.c_str();
  }

 private:
  std::string wh;
};
