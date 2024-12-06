#include <pep/core-client/CoreClient.ServerConnection.hpp>
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

rxcpp::observable<EnrollmentResult> CoreClient::enrollServer() {
  LOG(LOG_TAG, debug) << "Enrolling server...";
  auto ctx = std::make_shared<EnrollmentContext>();
  ctx->privateKey = std::make_shared<AsymmetricKey>(privateKey);
  ctx->certificateChain = certificateChain;

  return completeEnrollment(ctx);
}

rxcpp::observable<EnrollmentResult> CoreClient::completeEnrollment(std::shared_ptr<EnrollmentContext> ctx) {
  LOG(LOG_TAG, debug) << "Completing enrollment...";
  // Construct key component request for Access Manager and Transcryptor
  KeyComponentRequest kcReq;

  ctx->keyComponentRequest = SignedKeyComponentRequest(std::move(kcReq), ctx->certificateChain, *ctx->privateKey);

  // Send request to access manager
  return clientAccessManager->sendRequest<KeyComponentResponse>(ctx->keyComponentRequest)
    .flat_map([this, ctx](KeyComponentResponse lpResponse) {
      // Store returned key components in local context
      ctx->alpha = lpResponse.mPseudonymKeyComponent;
      ctx->beta = lpResponse.mEncryptionKeyComponent;
      ctx->enrollmentScheme = lpResponse.mEnrollmentScheme;

      // Send request to Transcryptor
        return clientTranscryptor->sendRequest<KeyComponentResponse>(ctx->keyComponentRequest);
    })
    .map([this, ctx](KeyComponentResponse lpResponse) {
      // Store returned key components in local context
      ctx->gamma = lpResponse.mPseudonymKeyComponent;
      ctx->delta = lpResponse.mEncryptionKeyComponent;
      if (lpResponse.mEnrollmentScheme != ctx->enrollmentScheme) {
        throw std::runtime_error("Enrollment schemes returned by access manager and transcryptor do not match");
      }

      // Compute final keys
      this->privateKeyPseudonyms = ctx->alpha.mult(ctx->gamma);
      this->privateKeyData = ctx->beta.mult(ctx->delta);

      // Store certificate chain
      privateKey = *ctx->privateKey;
      certificateChain = ctx->certificateChain;

      // Fire timer when our certificates expire in order to inform the user
      auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(certificateChain.front().getValidityDuration());
      rxcpp::observable<>::timer(expiry, rxcpp::observe_on_new_thread()).subscribe([this](auto) {
        registrationSubject.get_subscriber().on_next(1);
      });

      EnrollmentResult result;
      result.privateKeyData = privateKeyData;
      result.privateKeyPseudonyms = privateKeyPseudonyms;
      result.certificateChain = certificateChain;
      result.privateKey = privateKey;
      result.enrollmentScheme = ctx->enrollmentScheme;

      enrollmentSubject.get_subscriber().on_next(result);
      return result;
    });
}

std::string CoreClient::getEnrolledGroup() {
  if (certificateChain.empty())
    return "";
  return certificateChain.front().getOrganizationalUnit();
}

std::string CoreClient::getEnrolledUser() const {
  if (certificateChain.empty()) {
    return "";
  }
  return certificateChain.front().getCommonName();
}


bool CoreClient::getEnrolled() {
  if (certificateChain.empty()) {
    return false;
  }
  return certificateChain.front().getValidityDuration() > 0;
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
    config.add<std::string>("PrivateKey", privateKey.toPem());
  }
  if (writeCertificateChain) {
    config.add<std::string>("CertificateChain", certificateChain.toPem());
  }

  config.add<std::string>("EnrollmentScheme", Serialization::ToEnumString(enrollmentScheme));

  boost::property_tree::write_json(os, config);
}

}
