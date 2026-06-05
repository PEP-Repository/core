#include <pep/morphing/MorphingPropertySerializers.hpp>

#include <pep/elgamal/ElgamalPropertySerializers.hpp>
#include <pep/morphing/MorphingSerializers.hpp>
#include <pep/serialization/Serialization.hpp>
#include <pep/utils/MiscUtil.hpp>

namespace pep {

UnsupportedEnrollmentSchemeError::UnsupportedEnrollmentSchemeError(EnrollmentScheme scheme)
  : std::runtime_error("Unsupported EnrollmentScheme: " + std::string(Serialization::ToEnumString(scheme))),
    scheme_{scheme} {}

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
  auto privateKey = DeserializeProperties<std::optional<std::string>>(source, "PrivateKey", context);
  auto certificateChain = DeserializeProperties<std::optional<std::string>>(source, "CertificateChain", context);

  const bool isUserCertificate = privateKey || certificateChain;
  // Do not try to load old keys from before we introduced the enrollment scheme,
  //  or keys generated with a wrong enrollment scheme.
  // This only applies to user keys, see note in serializer above.
  // Note that we cannot reject server keys with a wrong EnrollmentScheme,
  //  because pepEnrollment used to add EnrollmentScheme for servers as well.
  if (isUserCertificate && scheme != EnrollmentScheme::Current) {
    throw UnsupportedEnrollmentSchemeError{scheme.value_or(EnrollmentScheme::V1)};
  }

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
