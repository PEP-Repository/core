#pragma once

#include <string>
#include <cstdint>

namespace pep {

std::string PackUint8(uint8_t x);
std::string PackUint32BE(uint32_t x);
std::string PackUint64BE(uint64_t x);
uint32_t UnpackUint32BE(const std::string& x);
uint64_t UnpackUint64BE(const std::string& x);

}
