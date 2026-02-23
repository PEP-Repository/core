#pragma once

#include <pep/key-components/KeyComponentServerProxy.hpp>
#include <pep/transcryptor/TranscryptorMessages.hpp>

namespace pep {

class TranscryptorProxy : public KeyComponentServerProxy {
public:
  using KeyComponentServerProxy::KeyComponentServerProxy;

  rxcpp::observable<TranscryptorResponse> requestTranscryption(TranscryptorRequest request, messaging::Tail<TranscryptorRequestEntries> entries) const;
  rxcpp::observable<RekeyResponse> requestRekey(RekeyRequest request) const;
  rxcpp::observable<LogIssuedTicketResponse> requestLogIssuedTicket(LogIssuedTicketRequest request) const;
};

}
