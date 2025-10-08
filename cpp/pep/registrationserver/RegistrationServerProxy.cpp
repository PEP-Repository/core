#include <pep/messaging/ResponseToVoid.hpp>
#include <pep/registrationserver/RegistrationServerProxy.hpp>
#include <pep/registrationserver/RegistrationServerSerializers.hpp>

namespace pep {

rxcpp::observable<std::string> RegistrationServerProxy::registerPepId() const {
  return this->sendRequest<PEPIdRegistrationResponse>(this->sign(PEPIdRegistrationRequest()))
    .op(RxGetOne("PEPIdRegistrationResponse"))
    .map([](const PEPIdRegistrationResponse& response) {return response.mPepId; });
}

rxcpp::observable<FakeVoid> RegistrationServerProxy::requestRegistration(RegistrationRequest request) const {
  return this->sendRequest<RegistrationResponse>(this->sign(std::move(request)))
    .op(messaging::ResponseToVoid());
}

rxcpp::observable<ListCastorImportColumnsResponse> RegistrationServerProxy::requestListCastorImportColumns(ListCastorImportColumnsRequest request) const {
  return this->sendRequest<ListCastorImportColumnsResponse>(std::move(request))
    .op(RxGetOne("ListCastorImportColumnsResponse"));
}

}
