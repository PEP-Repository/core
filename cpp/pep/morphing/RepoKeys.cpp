#include <pep/morphing/RepoKeys.hpp>

#include <pep/utils/BoostHexUtil.hpp>
#include <pep/utils/CollectionUtils.hpp>
#include <pep/utils/MiscUtil.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/property_tree/ptree.hpp>

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

  return SpanToArray(std::as_bytes(ToSizedSpan<ByteSize>(boost::algorithm::unhex(hex))));
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
