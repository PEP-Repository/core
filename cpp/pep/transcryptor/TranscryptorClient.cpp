#include <pep/async/RxUtils.hpp>
#include <pep/transcryptor/KeyComponentSerializers.hpp>
#include <pep/transcryptor/TranscryptorClient.hpp>
#include <pep/transcryptor/TranscryptorSerializers.hpp>

namespace pep {

rxcpp::observable<KeyComponentResponse> TranscryptorClient::requestKeyComponent(SignedKeyComponentRequest request) const {
  return this->sendRequest<KeyComponentResponse>(std::move(request))
    .op(RxGetOne("KeyComponentResponse"));
}

rxcpp::observable<TranscryptorResponse> TranscryptorClient::requestTranscryption(TranscryptorRequest request, MessageTail<TranscryptorRequestEntries> tail) const {
  return this->sendRequest<TranscryptorResponse>(std::move(request), std::move(tail))
    .op(RxGetOne("TranscryptorResponse"));
}

rxcpp::observable<RekeyResponse> TranscryptorClient::requestRekey(RekeyRequest request) const {
  return this->sendRequest<RekeyResponse>(std::move(request))
    .op(RxGetOne("RekeyResponse"));
}

rxcpp::observable<LogIssuedTicketResponse> TranscryptorClient::requestLogIssuedTicket(LogIssuedTicketRequest request) const {
  return this->sendRequest<LogIssuedTicketResponse>(std::move(request))
    .op(RxGetOne("LogIssuedTicketResponse"));

}

}
