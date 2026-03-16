#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <pep/utils/CollectionUtils.hpp>

namespace pep {

/// Compute HMAC according to RFC 2104
template <typename THasher>
THasher::Hash Hmac(std::string_view key, std::string_view data) {
  THasher finalHasher;
  const auto blockSize = finalHasher.blockSize();

  std::string kStr;
  // Check if key is longer than blocksize
  if (key.size() > blockSize) {
    // Shorten key by hashing it
    kStr = THasher{}.digest(key);
  }
  else {
    // Copy the provided key as-is
    kStr = key;
  }
  kStr.resize(blockSize); // Zero-pad

  // Construct padded keys
  std::vector<uint8_t> k_ipad, k_opad;
  k_ipad.reserve(blockSize);
  k_opad.reserve(blockSize);
  // Iterate as unsigned
  for (std::uint8_t b : ConvertBytes<std::uint8_t>(kStr)) {
    k_ipad.push_back(b ^ 0x36);
    k_opad.push_back(b ^ 0x5C);
  }

  // Compute inner hash
  auto intermediateHash = THasher{}.digest(SpanToString(k_ipad), SpanToString(data));

  // Compute final HMAC
  return finalHasher.digest(SpanToString(k_opad), SpanToString(intermediateHash));
}

}
