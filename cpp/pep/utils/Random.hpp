#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>
#include <vector>

namespace pep {

class RandomException : public std::exception {
 public:
  RandomException() {}
  const char* what() const noexcept override {
    return "Could not generate random data";
  }
};

void RandomBytes(std::string& s, size_t len);
void RandomBytes(uint8_t* r, uint64_t len); /* throws RandomException */
void RandomBytes(std::vector<char>& s, size_t len);
int MbedRandomSource(void* p_rng, unsigned char* output, size_t output_len);
std::string RandomString(size_t len);

template<size_t Length>
std::array<std::byte, Length> RandomArray() {
  std::array<std::byte, Length> buf;
  RandomBytes(reinterpret_cast<uint8_t*>(buf.data()), buf.size());
  return buf;
}

}
