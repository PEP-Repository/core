#include <pep/async/RxRequireCount.hpp>
#include <pep/transcryptor/KeyComponentSerializers.hpp>
#include <pep/transcryptor/TranscryptorProxy.hpp>
#include <pep/transcryptor/TranscryptorSerializers.hpp>

namespace pep {

rxcpp::observable<KeyComponentResponse> TranscryptorProxy::requestKeyComponent(SignedKeyComponentRequest request) const {
  return this->sendRequest<KeyComponentResponse>(std::move(request))
    .op(RxGetOne());
}

rxcpp::observable<TranscryptorResponse> TranscryptorProxy::requestTranscryption(TranscryptorRequest request, messaging::Tail<TranscryptorRequestEntries> entries) const {
  return this->sendRequest<TranscryptorResponse>(std::move(request), std::move(entries))
    .op(RxGetOne());
}

rxcpp::observable<RekeyResponse> TranscryptorProxy::requestRekey(RekeyRequest request) const {
  return this->sendRequest<RekeyResponse>(std::move(request))
    .op(RxGetOne());
}

rxcpp::observable<LogIssuedTicketResponse> TranscryptorProxy::requestLogIssuedTicket(LogIssuedTicketRequest request) const {
  return this->sendRequest<LogIssuedTicketResponse>(std::move(request))
    .op(RxGetOne());

}

}
