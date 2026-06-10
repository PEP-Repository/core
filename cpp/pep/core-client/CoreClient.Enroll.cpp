#include <pep/auth/OAuthToken.hpp>
#include <pep/core-client/CoreClient.hpp>
#include <pep/key-components/KeyComponentSerializers.hpp>
#include <pep/utils/Log.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <rxcpp/operators/rx-concat.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-observe_on.hpp>

namespace pep {

static const std::string LogTag ("CoreClient (enroll)");

CoreClient::EnrollmentContext::EnrollmentContext(std::shared_ptr<const X509Identity> enroller)
  : identity(std::move(enroller)), keyComponentRequest(KeyComponentRequest{}, *identity) {
}

rxcpp::observable<EnrolledPartyKeys> CoreClient::enrollServer() {
  PEP_LOG(LogTag, Severity::Debug) << "Enrolling server...";
  auto ctx = std::make_shared<EnrollmentContext>(this->getSigningIdentity());
  return completeEnrollment(ctx);
}

rxcpp::observable<EnrolledPartyKeys> CoreClient::completeEnrollment(std::shared_ptr<EnrollmentContext> ctx) {
  PEP_LOG(LogTag, Severity::Debug) << "Completing enrollment...";
  // Construct key component request for Access Manager and Transcryptor
  // Send request to access manager
  return getAccessManagerProxy(true)->requestKeyComponent(ctx->keyComponentRequest)
    .flat_map([this, ctx](KeyComponentResponse lpResponse) {
      // Store returned key components in local context
      ctx->pseudonymEncryptionKeyComponentAM = lpResponse.mPseudonymEncryptionKeyComponent;
      ctx->dataEncryptionKeyComponentAM = lpResponse.mDataEncryptionKeyComponent;

      // Send request to Transcryptor
        return getTranscryptorProxy(true)->requestKeyComponent(ctx->keyComponentRequest);
    })
    .map([this, ctx](KeyComponentResponse lpResponse) {
      // Store returned key components in local context
      ctx->pseudonymEncryptionKeyComponentTS = lpResponse.mPseudonymEncryptionKeyComponent;
      ctx->dataEncryptionKeyComponentTS = lpResponse.mDataEncryptionKeyComponent;

      // Compute final keys
      this->privateKeyPseudonyms = ctx->pseudonymEncryptionKeyComponentAM * ctx->pseudonymEncryptionKeyComponentTS;
      this->privateKeyData = ctx->dataEncryptionKeyComponentAM * ctx->dataEncryptionKeyComponentTS;

      // Store identity
      this->setSigningIdentity(ctx->identity);

      // First convert the certificate expiry in system time to a steady clock
      auto durationUntilExpiry = ctx->identity->getCertificateChain().leaf().getNotAfter() - std::chrono::system_clock::now();
      std::chrono::time_point<std::chrono::steady_clock> steadyExpiry = std::chrono::steady_clock::now() + durationUntilExpiry;

      // Then fire a timer when our certificates expire in order to inform the user
      rxcpp::observable<>::timer(steadyExpiry, rxcpp::observe_on_new_thread()).subscribe([this](auto) {
        registrationSubject.get_subscriber().on_next(1);
      });

      EnrolledPartyKeys result{
        .pseudonymKey = privateKeyPseudonyms,
        .dataKey = privateKeyData != CurveScalar{} ? std::optional{privateKeyData} : std::nullopt,
        .signingIdentity = *ctx->identity,
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

}
