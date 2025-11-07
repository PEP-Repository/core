#pragma once

#include <pep/server/SigningServerProxy.hpp>
#include <pep/transcryptor/KeyComponentMessages.hpp>
#include <pep/transcryptor/TranscryptorMessages.hpp>

namespace pep {

class TranscryptorProxy : public SigningServerProxy {
public:
  using SigningServerProxy::SigningServerProxy;

  rxcpp::observable<KeyComponentResponse> requestKeyComponent(SignedKeyComponentRequest request) const;
  rxcpp::observable<TranscryptorResponse> requestTranscryption(TranscryptorRequest request, messaging::Tail<TranscryptorRequestEntries> entries) const;
  rxcpp::observable<RekeyResponse> requestRekey(RekeyRequest request) const;
  rxcpp::observable<LogIssuedTicketResponse> requestLogIssuedTicket(LogIssuedTicketRequest request) const;
};

}
