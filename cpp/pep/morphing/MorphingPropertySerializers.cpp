#include <pep/morphing/MorphingPropertySerializers.hpp>

#include <pep/elgamal/ElgamalPropertySerializers.hpp>
#include <pep/morphing/MorphingSerializers.hpp>
#include <pep/serialization/Serialization.hpp>
#include <pep/utils/MiscUtil.hpp>

namespace pep {

void PropertySerializer<EnrolledPartyKeys>::write(boost::property_tree::ptree& destination, const EnrolledPartyKeys& value) const {
  SerializeProperties(destination, "PseudonymKey", value.pseudonymKey);
  SerializeProperties(destination, "DataKey", value.dataKey);
  if (value.signingIdentity) {
    // EnrollmentScheme only makes sense for users, not servers, which use static keys independent of the certificate
    SerializeProperties(destination, "EnrollmentScheme", std::string(Serialization::ToEnumString(EnrollmentScheme::Current)));
    SerializeProperties(destination, "PrivateKey", value.signingIdentity->getPrivateKey().toPem());
    SerializeProperties(destination, "CertificateChain", X509CertificatesToPem(value.signingIdentity->getCertificateChain().certificates()));
  }
}
EnrolledPartyKeys PropertySerializer<EnrolledPartyKeys>::read(const boost::property_tree::ptree& source, const DeserializationContext& context) const {
  const auto scheme = GetOptionalValue(
    DeserializeProperties<std::optional<std::string>>(source, "EnrollmentScheme", context),
    Serialization::ParseEnum<EnrollmentScheme>);
  // Do not try to load a file with a wrong enrollment scheme
  if (scheme && *scheme != EnrollmentScheme::Current) { return {}; }

  auto privateKey = DeserializeProperties<std::optional<std::string>>(source, "PrivateKey", context);
  auto certificateChain = DeserializeProperties<std::optional<std::string>>(source, "CertificateChain", context);
  // Do not try to load a user keys file with no enrollment scheme or a wrong scheme
  if ((privateKey || certificateChain) && scheme != EnrollmentScheme::Current) { return {}; }
  return {
    .pseudonymKey = DeserializeProperties<std::optional<ElgamalPrivateKey>>(source, "PseudonymKey", context),
    .dataKey = DeserializeProperties<std::optional<ElgamalPrivateKey>>(source, "DataKey", context),
    .signingIdentity = privateKey && certificateChain
      ? std::optional{X509Identity(AsymmetricKey(*privateKey), X509CertificateChain(X509CertificatesFromPem(*certificateChain)))}
      : std::nullopt,
  };
}

void PropertySerializer<SystemPublicKeys>::write(boost::property_tree::ptree& destination, const SystemPublicKeys& value) const {
  SerializeProperties(destination, "PublicKeyPseudonyms", value.globalPseudonymEncryptionKey);
  SerializeProperties(destination, "PublicKeyData", value.globalDataEncryptionKey);
}
SystemPublicKeys PropertySerializer<SystemPublicKeys>::read(const boost::property_tree::ptree& source, const DeserializationContext& context) const {
  return {
    .globalPseudonymEncryptionKey = DeserializeProperties<ElgamalPublicKey>(source, "PublicKeyPseudonyms", context),
    .globalDataEncryptionKey = DeserializeProperties<ElgamalPublicKey>(source, "PublicKeyData", context),
  };
}

}
