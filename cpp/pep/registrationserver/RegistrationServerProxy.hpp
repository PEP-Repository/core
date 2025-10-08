#pragma once

#include <pep/registrationserver/RegistrationServerMessages.hpp>
#include <pep/server/SigningServerProxy.hpp>

namespace pep {

class RegistrationServerProxy : public SigningServerProxy {
public:
  using SigningServerProxy::SigningServerProxy;

  rxcpp::observable<RegistrationResponse> requestRegistration(RegistrationRequest request) const;
  rxcpp::observable<ListCastorImportColumnsResponse> requestListCastorImportColumns(ListCastorImportColumnsRequest request) const;

  rxcpp::observable<std::string> registerPepId() const;
};

}
