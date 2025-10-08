#pragma once

#include <pep/server/SigningServerProxy.hpp>
#include <pep/transcryptor/KeyComponentMessages.hpp>
#include <pep/transcryptor/TranscryptorMessages.hpp>

namespace pep {

class TranscryptorClient : public SigningServerProxy {
public:
  using SigningServerProxy::SigningServerProxy;

  rxcpp::observable<KeyComponentResponse> requestKeyComponent(SignedKeyComponentRequest request) const;
  rxcpp::observable<TranscryptorResponse> requestTranscryption(TranscryptorRequest request, MessageTail<TranscryptorRequestEntries> entries) const;
  rxcpp::observable<RekeyResponse> requestRekey(RekeyRequest request) const;
  rxcpp::observable<LogIssuedTicketResponse> requestLogIssuedTicket(LogIssuedTicketRequest request) const;
};

}
