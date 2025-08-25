#pragma once

#include <cstdint>
#include <string>

namespace pep {

struct EndPoint {
  std::string hostname;
  uint16_t port{};
  std::string expectedCommonName;

  EndPoint() = default;
  EndPoint(
      std::string hostname,
      uint16_t port,
      std::string expectedCommonName = "")
    : hostname(std::move(hostname)), port{port}, expectedCommonName(std::move(expectedCommonName))
  {}

  std::string describe() const {
    return this->expectedCommonName.empty() 
      ? this->hostname 
      : this->expectedCommonName;
  }
};

}
