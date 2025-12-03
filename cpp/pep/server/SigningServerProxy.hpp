#pragma once

#include <pep/messaging/HousekeepingMessages.hpp>
#include <pep/server/ServerProxy.hpp>

namespace pep {

class SigningServerProxy : public ServerProxy {
private:
  rxcpp::observable<SignedPingResponse> requestSignedPing(const PingRequest& request) const;

public:
  using ServerProxy::ServerProxy;

  rxcpp::observable<PingResponse> requestPing() const override;
  rxcpp::observable<X509CertificateChain> requestCertificateChain() const;
  rxcpp::observable<X509CertificateSigningRequest> requestCertificateSigningRequest() const;
  rxcpp::observable<FakeVoid> requestCertificateReplacement(X509CertificateChain newCertificateChain, bool force);
};

}
