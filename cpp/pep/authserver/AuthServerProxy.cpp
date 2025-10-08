#include <pep/async/RxGetOne.hpp>
#include <pep/authserver/AuthServerProxy.hpp>
#include <pep/authserver/AuthserverSerializers.hpp>

namespace pep {

rxcpp::observable<std::string> AuthServerProxy::requestToken(std::string subject, std::string group, Timestamp expirationTime) const {
  TokenRequest request(std::move(subject), std::move(group), std::move(expirationTime));
  return this->sendRequest<TokenResponse>(this->sign(std::move(request)))
    .op(RxGetOne("TokenResponse"))
    .map([](TokenResponse response) { return response.mToken; });
}

}
