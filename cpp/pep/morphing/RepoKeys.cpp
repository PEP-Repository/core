#include <pep/morphing/RepoKeys.hpp>

#include <pep/utils/BoostHexUtil.hpp>
#include <pep/utils/MiscUtil.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/property_tree/ptree.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>

using namespace pep;

namespace {

template<size_t ByteSize>
auto ParseKey(const std::string& hex) {
  if (hex.length() != BoostHexLength(ByteSize)) {
    throw std::invalid_argument("Unexpected key length");
  }

  std::string key = boost::algorithm::unhex(hex);
  assert(key.length() == ByteSize);

  std::array<std::byte, ByteSize> result;
  std::ranges::copy(std::as_bytes(std::span(key)), result.begin());
  return result;
}

} // namespace


PseudonymTranslationKeys pep::ParsePseudonymTranslationKeys(const boost::property_tree::ptree& config) {
  return {
    .encryptionKeyFactorSecret{ParseKey<64>(config.get<std::string>("PseudonymsRekeyLocal"))},
    .pseudonymizationKeyFactorSecret{ParseKey<64>(config.get<std::string>("PseudonymsReshuffleLocal"))},
    .masterPrivateEncryptionKeyShare{ParseKey<32>(config.get<std::string>("MasterPrivateKeySharePseudonyms"))},
  };
}

DataTranslationKeys pep::ParseDataTranslationKeys(const boost::property_tree::ptree& config) {
  return {
    .encryptionKeyFactorSecret{ParseKey<64>(config.get<std::string>("DataRekeyLocal"))},
    .blindingKeySecret = decltype(DataTranslationKeys::blindingKeySecret)(ConvertOptional(config.get_optional<std::string>("DataBlinding").map(ParseKey<64>))),
    .masterPrivateEncryptionKeyShare{ParseKey<32>(config.get<std::string>("MasterPrivateKeyShareData"))},
  };
}
