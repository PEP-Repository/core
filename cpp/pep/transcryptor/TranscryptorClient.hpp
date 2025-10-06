#pragma once

#include <pep/server/SigningServerClient.hpp>
#include <pep/transcryptor/KeyComponentMessages.hpp>
#include <pep/transcryptor/TranscryptorMessages.hpp>

namespace pep {

class TranscryptorClient : public SigningServerClient {
public:
  using SigningServerClient::SigningServerClient;

  rxcpp::observable<KeyComponentResponse> requestKeyComponent() const;
  rxcpp::observable<TranscryptorResponse> requestTranscryption(TranscryptorRequest request, MessageTail<TranscryptorRequestEntries> tail) const;
  rxcpp::observable<RekeyResponse> requestRekey(RekeyRequest request) const;
  rxcpp::observable<LogIssuedTicketResponse> requestLogIssuedTicket(LogIssuedTicketRequest request) const;
};

}
