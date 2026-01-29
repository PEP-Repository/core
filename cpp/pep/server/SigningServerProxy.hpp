#pragma once

#include <pep/messaging/HousekeepingMessages.hpp>
#include <pep/server/ServerProxy.hpp>

namespace pep {

class SigningServerProxy : public ServerProxy {
private:
  std::string mExpectedCommonName;
  std::shared_ptr<X509RootCertificates> mRootCertificates;

  void assertValidCertificateChain(const X509CertificateChain& chain, bool allowChangingSubject) const;

  rxcpp::observable<SignedPingResponse> requestSignedPing(const PingRequest& request) const;

public:
  /// @brief Constructor
  /// @param untyped The ServerConnection that can exchange messages with the proxied server
  /// @param clientMessageSigner The instance that will sign messages sent to the server.
  /// @param expectedCommonName The expected common name in certificates of signed messages from the server.
  /// @param rootCertificates The rootCertificates that can be used to verify signed messages
  /// @remark Caller must ensure that the MessageSigner outlives the ServerProxy
  SigningServerProxy(std::shared_ptr<messaging::ServerConnection> untyped, const MessageSigner& clientMessageSigner,
    std::string expectedCommonName, std::shared_ptr<X509RootCertificates> rootCertificates)
    : ServerProxy(std::move(untyped), clientMessageSigner), mExpectedCommonName(std::move(expectedCommonName)), mRootCertificates(std::move(rootCertificates)) {}

  const std::string& getExpectedCommonName() const { return mExpectedCommonName; }

  rxcpp::observable<PingResponse> requestPing() const override;
  rxcpp::observable<X509CertificateChain> requestCertificateChain() const;
  rxcpp::observable<X509CertificateSigningRequest> requestCertificateSigningRequest() const;
  rxcpp::observable<FakeVoid> requestCertificateReplacement(const X509CertificateChain& newCertificateChain, bool allowChangingSubject) const;
  rxcpp::observable<FakeVoid> commitCertificateReplacement(const X509CertificateChain& newCertificateChain) const;
};

}
