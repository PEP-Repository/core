#include <pep/core-client/CoreClient.hpp>
#include <pep/transcryptor/KeyComponentSerializers.hpp>
#include <pep/auth/OAuthToken.hpp>
#include <pep/utils/Log.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <rxcpp/operators/rx-concat.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-observe_on.hpp>

namespace pep {

static const std::string LOG_TAG ("CoreClient (enroll)");

CoreClient::EnrollmentContext::EnrollmentContext(std::shared_ptr<const X509Identity> enroller)
  : identity(std::move(enroller)), keyComponentRequest(KeyComponentRequest{}, *identity) {
}

rxcpp::observable<EnrollmentResult> CoreClient::enrollServer() {
  LOG(LOG_TAG, debug) << "Enrolling server...";
  auto ctx = std::make_shared<EnrollmentContext>(this->getSigningIdentity());
  return completeEnrollment(ctx);
}

rxcpp::observable<EnrollmentResult> CoreClient::completeEnrollment(std::shared_ptr<EnrollmentContext> ctx) {
  LOG(LOG_TAG, debug) << "Completing enrollment...";
  // Construct key component request for Access Manager and Transcryptor
  // Send request to access manager
  return accessManagerProxy->requestKeyComponent(ctx->keyComponentRequest)
    .flat_map([this, ctx](KeyComponentResponse lpResponse) {
      // Store returned key components in local context
      ctx->alpha = lpResponse.mPseudonymKeyComponent;
      ctx->beta = lpResponse.mEncryptionKeyComponent;

      // Send request to Transcryptor
        return transcryptorProxy->requestKeyComponent(ctx->keyComponentRequest);
    })
    .map([this, ctx](KeyComponentResponse lpResponse) {
      // Store returned key components in local context
      ctx->gamma = lpResponse.mPseudonymKeyComponent;
      ctx->delta = lpResponse.mEncryptionKeyComponent;

      // Compute final keys
      this->privateKeyPseudonyms = ctx->alpha.mult(ctx->gamma);
      this->privateKeyData = ctx->beta.mult(ctx->delta);

      // Store identity
      this->setSigningIdentity(ctx->identity);

      // First convert the certificate expiry in system time to a steady clock
      auto durationUntilExpiry = ctx->identity->getCertificateChain().leaf().getNotAfter() - std::chrono::system_clock::now();
      std::chrono::time_point<std::chrono::steady_clock> steadyExpiry = std::chrono::steady_clock::now() + durationUntilExpiry;

      // Then fire a timer when our certificates expire in order to inform the user
      rxcpp::observable<>::timer(steadyExpiry, rxcpp::observe_on_new_thread()).subscribe([this](auto) {
        registrationSubject.get_subscriber().on_next(1);
      });

      EnrollmentResult result{
        .privateKeyData = privateKeyData,
        .privateKeyPseudonyms = privateKeyPseudonyms,
        .signingIdentity = *ctx->identity
      };

      enrollmentSubject.get_subscriber().on_next(result);
      return result;
    });
}

std::string CoreClient::getEnrolledGroup() {
  if (auto signingIdentity = this->getSigningIdentity(false)) {
    return signingIdentity->getCertificateChain().leaf().getOrganizationalUnit().value_or("");
  }
  return "";
}

std::string CoreClient::getEnrolledUser() const {
  if (auto signingIdentity = this->getSigningIdentity(false)) {
    return signingIdentity->getCertificateChain().leaf().getCommonName().value_or("");
  }
  return "";
}


bool CoreClient::getEnrolled() {
  if (auto signingIdentity = this->getSigningIdentity(false)) {
    return signingIdentity->getCertificateChain().leaf().isCurrentTimeInValidityPeriod();
  }
  return false;
}

void EnrollmentResult::writeJsonTo(std::ostream& os, bool writeDataKey, bool writePrivateKey, bool writeCertificateChain) const {
  boost::property_tree::ptree config;

  config.add<std::string>("PseudonymKey",
      boost::algorithm::hex(privateKeyPseudonyms.pack()));

  if (writeDataKey) {
    config.add<std::string>("DataKey",
      boost::algorithm::hex(privateKeyData.pack()));
  }
  if (writePrivateKey) {
    config.add<std::string>("PrivateKey", signingIdentity.getPrivateKey().toPem());
  }
  if (writeCertificateChain) {
    config.add<std::string>("CertificateChain", X509CertificatesToPem(signingIdentity.getCertificateChain().certificates()));
  }

  config.add<std::string>("EnrollmentScheme", std::string(Serialization::ToEnumString(ENROLLMENT_SCHEME_CURRENT)));

  boost::property_tree::write_json(os, config);
}

}
