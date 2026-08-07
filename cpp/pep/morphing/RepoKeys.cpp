#include <pep/morphing/RepoKeys.hpp>

#include <pep/utils/CollectionUtils.hpp>
#include <pep/crypto/ConstTime.hpp>
#include <pep/utils/MiscUtil.hpp>

#include <boost/property_tree/ptree.hpp>

#include <string>

using namespace pep;

namespace {

template<size_t ByteSize>
auto ParseKey(std::string_view hex) {
  return SpanToArray(std::as_bytes(ToSizedSpan<ByteSize>(const_time::FromHex(hex))));
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
