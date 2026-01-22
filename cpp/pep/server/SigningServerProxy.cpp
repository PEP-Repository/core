#include <pep/async/RxRequireCount.hpp>
#include <pep/messaging/MessagingSerializers.hpp>
#include <pep/messaging/ResponseToVoid.hpp>
#include <pep/server/SigningServerProxy.hpp>
#include <pep/server/CertificateRenewalMessages.hpp>
#include <pep/server/CertificateRenewalSerializers.hpp>

namespace pep {
void SigningServerProxy::assertValidCertificateChain(const X509CertificateChain& chain, bool allowChangingSubject) const {
  // Validity will also be checked by the server, but is safer to check that both client and server agree that the certificate is valid.
  if (!allowChangingSubject && chain.leaf().getCommonName() != mExpectedCommonName) {
    throw std::runtime_error(std::format("Certificate chain has common name {} but the expected common name is {}", chain.leaf().getCommonName().value_or("<NOT SET>"), mExpectedCommonName));
  }
  if (!chain.verify(*mRootCertificates)) {
    throw std::runtime_error("Certificate chain is not valid");
  }
}

rxcpp::observable<SignedPingResponse> SigningServerProxy::requestSignedPing(const PingRequest& request) const {
  return this->sendRequest<SignedPingResponse>(request)
    .op(RxGetOne());
}

rxcpp::observable<PingResponse> SigningServerProxy::requestPing() const {
  return this->requestSignedPing(PingRequest{})
    .map([](const SignedPingResponse& response) { return response.openWithoutCheckingSignature(); });
}

rxcpp::observable<X509CertificateChain> SigningServerProxy::requestCertificateChain() const {
  PingRequest request;
  return this->requestSignedPing(request)
    .map([request](const SignedPingResponse& response) {
    response.openWithoutCheckingSignature().validate(request);
    return response.mSignature.mCertificateChain;
      });

}

rxcpp::observable<X509CertificateSigningRequest> SigningServerProxy::requestCertificateSigningRequest() const {
  return this->sendRequest<SignedCsrResponse>(this->sign(CsrRequest{}))
  .op(RxGetOne("Signed CSR Response"))
  .map([rootCAs=mRootCertificates, expectedCommonName=mExpectedCommonName](const SignedCsrResponse& signedResponse) {
    auto response = signedResponse.open(*rootCAs, expectedCommonName);
    if (response.getCsr().getCommonName() != expectedCommonName) {
      throw std::runtime_error("Received certificate signing request does not have expected common name");
    }
    if (!response.getCsr().verifySignature()) {
      throw std::runtime_error("Received certificate signing request does not have a valid signature");
    }
    return response.getCsr();
  });
}

rxcpp::observable<FakeVoid> SigningServerProxy::requestCertificateReplacement(const X509CertificateChain& newCertificateChain, bool allowChangingSubject) const {
  assertValidCertificateChain(newCertificateChain, allowChangingSubject);
  return this->sendRequest<SignedCertificateReplacementResponse>(this->sign(CertificateReplacementRequest{newCertificateChain, allowChangingSubject}))
  .op(RxGetOne("Signed Certificate Replacement Response"))
  .map([rootCAs=mRootCertificates, expectedCommonName=mExpectedCommonName, newCertificateChain](const SignedCertificateReplacementResponse& signedResponse) {
    signedResponse.validate(*rootCAs, expectedCommonName);
    if (signedResponse.mSignature.mCertificateChain != newCertificateChain) {
      throw std::runtime_error("The response from the server was not signed by the new certificate chain");
    }
    return FakeVoid{};
  });
}

rxcpp::observable<FakeVoid> SigningServerProxy::commitCertificateReplacement(const X509CertificateChain& newCertificateChain) const{
  assertValidCertificateChain(newCertificateChain, true);
  return this->sendRequest<CertificateReplacementCommitResponse>(this->sign(CertificateReplacementCommitRequest{ newCertificateChain }))
  .op(messaging::ResponseToVoid());
}
}
