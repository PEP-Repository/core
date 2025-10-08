#include <pep/async/RxUtils.hpp>
#include <pep/transcryptor/KeyComponentSerializers.hpp>
#include <pep/transcryptor/TranscryptorProxy.hpp>
#include <pep/transcryptor/TranscryptorSerializers.hpp>

namespace pep {

rxcpp::observable<KeyComponentResponse> TranscryptorProxy::requestKeyComponent(SignedKeyComponentRequest request) const {
  return this->sendRequest<KeyComponentResponse>(std::move(request))
    .op(RxGetOne("KeyComponentResponse"));
}

rxcpp::observable<TranscryptorResponse> TranscryptorProxy::requestTranscryption(TranscryptorRequest request, MessageTail<TranscryptorRequestEntries> entries) const {
  return this->sendRequest<TranscryptorResponse>(std::move(request), std::move(entries))
    .op(RxGetOne("TranscryptorResponse"));
}

rxcpp::observable<RekeyResponse> TranscryptorProxy::requestRekey(RekeyRequest request) const {
  return this->sendRequest<RekeyResponse>(std::move(request))
    .op(RxGetOne("RekeyResponse"));
}

rxcpp::observable<LogIssuedTicketResponse> TranscryptorProxy::requestLogIssuedTicket(LogIssuedTicketRequest request) const {
  return this->sendRequest<LogIssuedTicketResponse>(std::move(request))
    .op(RxGetOne("LogIssuedTicketResponse"));

}

}
