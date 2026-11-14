#include <pep/utils/Raw.hpp>
#include <pep/storagefacility/PageHash.hpp>

#include <xxhash.h>

#include <sstream>
#include <cstdint>
#include <string>
#include <utility>

namespace pep
{

std::string XXHashStr(const std::string& data) {
  uint64_t xxhash = XXH64(data.data(), data.length(), 0);
  std::ostringstream out;
  WriteBinary(out, xxhash);
  return std::move(out).str();
}

std::string PageHash(const std::string& data) {
  return ETag(data, XXHashStr(data));
}

}

