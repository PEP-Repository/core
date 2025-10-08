#include <pep/async/RxUtils.hpp>
#include <pep/authserver/AuthServerProxy.hpp>
#include <pep/authserver/AuthserverSerializers.hpp>

namespace pep {

rxcpp::observable<TokenResponse> AuthServerProxy::requestToken(TokenRequest request) const {
  return this->sendRequest<TokenResponse>(this->sign(std::move(request)))
    .op(RxGetOne("TokenResponse"));
}

}
