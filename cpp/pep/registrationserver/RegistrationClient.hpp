#pragma once

#include <pep/registrationserver/RegistrationServerMessages.hpp>
#include <pep/server/SigningServerClient.hpp>

namespace pep {

class RegistrationClient : public SigningServerClient {
public:
  using SigningServerClient::SigningServerClient;

  rxcpp::observable<PEPIdRegistrationResponse> requestIdRegistration() const;
  rxcpp::observable<RegistrationResponse> requestRegistration(RegistrationRequest request) const;
  rxcpp::observable<ListCastorImportColumnsResponse> requestListCastorImportColumns(ListCastorImportColumnsRequest request) const;
};

}
