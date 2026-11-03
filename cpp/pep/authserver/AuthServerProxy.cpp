#include <pep/async/RxRequireCount.hpp>
#include <pep/authserver/AuthServerProxy.hpp>
#include <pep/authserver/AuthserverSerializers.hpp>
#include <pep/authserver/AuthserverMessages.hpp> // for TokenRequest, TokenResponse
#include <pep/utils/Timestamp.hpp>

#include <rxcpp/rx-lite.hpp>

#include <string>
#include <utility>

namespace pep {

rxcpp::observable<std::string> AuthServerProxy::requestToken(std::string subject, std::string group, Timestamp expirationTime) const {
  TokenRequest request{ std::move(subject), std::move(group), expirationTime };
  return this->sendRequest<TokenResponse>(this->sign(std::move(request)))
    .op(RxGetOne())
    .map([](TokenResponse response) { return response.token; });
}

}
