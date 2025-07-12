#include <pep/utils/Raw.hpp>

#include <pep/utils/Bitpacking.hpp>

namespace pep {

void WriteBinary(std::ostream& out, uint32_t number) {
  out << PackUint32BE(number);
}
void WriteBinary(std::ostream& out, uint64_t number) {
  out << PackUint64BE(number);
}
void WriteBinary(std::ostream& out, const std::string& str) {
  WriteBinary(out, static_cast<uint32_t>(str.length()));
  out.write(str.data(), static_cast<std::streamsize>(str.length()));
}

uint32_t ReadBinary(std::istream& in, uint32_t defaultValue) {
  std::string packed(sizeof(uint32_t), '\0');
  in.read(packed.data(), static_cast<std::streamsize>(packed.size()));
  return in.good() ? UnpackUint32BE(packed) : defaultValue;
}
uint64_t ReadBinary(std::istream& in, uint64_t defaultValue) {
  std::string packed(sizeof(uint64_t), '\0');
  in.read(packed.data(), static_cast<std::streamsize>(packed.size()));
  return in.good() ? UnpackUint64BE(packed) : defaultValue;
}
std::string ReadBinary(std::istream& in, const std::string& defaultValue) {
  uint32_t length = ReadBinary(in, uint32_t{ 0 });
  std::string retval(length, '\0');
  in.read(retval.data(), static_cast<std::streamsize>(length));
  return in.good() ? retval : defaultValue;
}

}
