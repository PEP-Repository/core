#pragma once

#include <pep/key-components/EnrollmentServerProxy.hpp>
#include <pep/transcryptor/TranscryptorMessages.hpp>

namespace pep {

class TranscryptorProxy : public EnrollmentServerProxy {
public:
  using EnrollmentServerProxy::EnrollmentServerProxy;

  rxcpp::observable<TranscryptorResponse> requestTranscryption(TranscryptorRequest request, messaging::Tail<TranscryptorRequestEntries> entries) const;
  rxcpp::observable<RekeyResponse> requestRekey(RekeyRequest request) const;
  rxcpp::observable<LogIssuedTicketResponse> requestLogIssuedTicket(LogIssuedTicketRequest request) const;
};

}
