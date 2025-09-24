#include <pep/client/Client.hpp>
#include <pep/keyserver/KeyServerSerializers.hpp>

namespace pep {

rxcpp::observable<TokenBlockingCreateResponse> Client::tokenBlockCreate(
    tokenBlocking::TokenIdentifier target,
    std::string note) {
  return clientKeyServer->sendRequest<TokenBlockingCreateResponse>(
      sign(TokenBlockingCreateRequest{std::move(target), std::move(note)}));
}

rxcpp::observable<TokenBlockingRemoveResponse> Client::tokenBlockRemove(int64_t id) {
  return clientKeyServer->sendRequest<TokenBlockingRemoveResponse>(sign(TokenBlockingRemoveRequest{id}));
}

rxcpp::observable<TokenBlockingListResponse> Client::tokenBlockList() {
  return clientKeyServer->sendRequest<TokenBlockingListResponse>(sign(TokenBlockingListRequest{}));
}

} // namespace pep
