#pragma once

#include <pep/registrationserver/RegistrationServerMessages.hpp>
#include <pep/server/SigningServerProxy.hpp>

namespace pep {

class RegistrationServerProxy : public SigningServerProxy {
public:
  using SigningServerProxy::SigningServerProxy;

  rxcpp::observable<std::string> registerPepId() const;
  rxcpp::observable<FakeVoid> completeShortPseudonyms(PolymorphicPseudonym pp, const std::string& identifier, const pep::AsymmetricKey& publicKeyShadowAdministration) const;
  rxcpp::observable<std::string> listCastorImportColumns(const std::string& spColumnName, const std::optional<unsigned>& answerSetCount) const;
};

}
