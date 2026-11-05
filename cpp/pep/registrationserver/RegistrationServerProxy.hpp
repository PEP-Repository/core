#pragma once

#include <pep/async/FakeVoid.hpp>
#include <pep/crypto/AsymmetricKey.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/server/SigningServerProxy.hpp>

#include <optional>
#include <string>

namespace pep {

class RegistrationServerProxy : public SigningServerProxy {
public:
  using SigningServerProxy::SigningServerProxy;

  rxcpp::observable<std::string> registerPepId() const;
  rxcpp::observable<FakeVoid> completeShortPseudonyms(PolymorphicPseudonym pp, const std::string& identifier, const pep::AsymmetricKey& publicKeyShadowAdministration) const;
  rxcpp::observable<std::string> listCastorImportColumns(const std::string& spColumnName, const std::optional<unsigned>& answerSetCount) const;
};

}
