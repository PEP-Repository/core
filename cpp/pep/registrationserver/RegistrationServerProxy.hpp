#pragma once

#include <pep/registrationserver/RegistrationServerMessages.hpp>
#include <pep/server/SigningServerProxy.hpp>

namespace pep {

class RegistrationServerProxy : public SigningServerProxy {
public:
  using SigningServerProxy::SigningServerProxy;

  rxcpp::observable<FakeVoid> requestRegistration(RegistrationRequest request) const;

  rxcpp::observable<std::string> registerPepId() const;
  rxcpp::observable<std::string> listCastorImportColumns(const std::string& spColumnName, const std::optional<unsigned>& answerSetCount) const;
};

}
