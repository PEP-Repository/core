#include <pep/async/RxUtils.hpp>
#include <pep/registrationserver/RegistrationClient.hpp>
#include <pep/registrationserver/RegistrationServerSerializers.hpp>

namespace pep {

rxcpp::observable<PEPIdRegistrationResponse> RegistrationClient::requestIdRegistration() const {
  return this->sendRequest<PEPIdRegistrationResponse>(this->sign(PEPIdRegistrationRequest()))
    .op(RxGetOne("PEPIdRegistrationResponse"));
}

rxcpp::observable<RegistrationResponse> RegistrationClient::requestRegistration(RegistrationRequest request) const {
  return this->sendRequest<RegistrationResponse>(this->sign(std::move(request)))
    .op(RxGetOne("RegistrationResponse"));
}

rxcpp::observable<ListCastorImportColumnsResponse> RegistrationClient::requestListCastorImportColumns(ListCastorImportColumnsRequest request) const {
  return this->sendRequest<ListCastorImportColumnsResponse>(std::move(request))
    .op(RxGetOne("ListCastorImportColumnsResponse"));
}

}
