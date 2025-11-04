#include <pep/messaging/ResponseToVoid.hpp>
#include <pep/registrationserver/RegistrationServerProxy.hpp>
#include <pep/registrationserver/RegistrationServerSerializers.hpp>

#include <rxcpp/operators/rx-flat_map.hpp>

namespace pep {

rxcpp::observable<std::string> RegistrationServerProxy::registerPepId() const {
  return this->sendRequest<PEPIdRegistrationResponse>(this->sign(PEPIdRegistrationRequest()))
    .op(RxGetOne())
    .map([](const PEPIdRegistrationResponse& response) {return response.mPepId; });
}

rxcpp::observable<FakeVoid> RegistrationServerProxy::completeShortPseudonyms(PolymorphicPseudonym pp, const std::string& identifier, const pep::AsymmetricKey& publicKeyShadowAdministration) const {
  RegistrationRequest request(pp);
  request.mEncryptedIdentifier = publicKeyShadowAdministration.encrypt(identifier);
  request.mEncryptionPublicKeyPem = publicKeyShadowAdministration.toPem();

  return this->sendRequest<RegistrationResponse>(this->sign(std::move(request)))
    .op(messaging::ResponseToVoid());
}

rxcpp::observable<std::string> RegistrationServerProxy::listCastorImportColumns(const std::string& spColumnName, const std::optional<unsigned>& answerSetCount) const {
  ListCastorImportColumnsRequest request{ spColumnName, answerSetCount.value_or(0U) };
  return this->sendRequest<ListCastorImportColumnsResponse>(std::move(request))
    .op(RxGetOne())
    .flat_map([](const ListCastorImportColumnsResponse& response) {
    return rxcpp::observable<>::iterate(response.mImportColumns);
      });
}


}
