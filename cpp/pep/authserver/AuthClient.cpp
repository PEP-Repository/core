#include <pep/async/RxUtils.hpp>
#include <pep/authserver/AuthClient.hpp>
#include <pep/authserver/AuthserverSerializers.hpp>

namespace pep {

rxcpp::observable<TokenResponse> AuthClient::requestToken(TokenRequest request) const {
  return this->sendRequest<TokenResponse>(this->sign(std::move(request)))
    .op(RxGetOne("TokenResponse"));
}

}
