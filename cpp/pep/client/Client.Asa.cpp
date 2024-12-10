#include <pep/authserver/AsaSerializers.hpp>
#include <pep/client/Client.hpp>
#include <pep/core-client/CoreClient.ServerConnection.hpp>

namespace pep {

namespace {

std::shared_ptr<Client::ServerConnection> EnsureAuthServerConnected(std::shared_ptr<Client::ServerConnection>
                                                                        authServer) {
  if (!authServer) {
    throw std::runtime_error("Authserver connection is not initialized. Does the client configuration contain correct config for the authserver endpoint?");
  }
  return authServer;
}

} // namespace

rxcpp::observable<FakeVoid> Client::asaRequestMutation(AsaMutationRequest request) {
  return EnsureAuthServerConnected(clientAuthserver)
      ->sendRequest<AsaMutationResponse>(sign(std::move(request))) // linebreak
      .map([](AsaMutationResponse resp) { return FakeVoid{}; });
}

rxcpp::observable<FakeVoid> Client::asaCreateUser(std::string uid) {
  AsaMutationRequest request;
  request.mCreateUser.emplace_back(std::move(uid));
  return asaRequestMutation(std::move(request));
}

rxcpp::observable<FakeVoid> Client::asaRemoveUser(std::string uid) {
  AsaMutationRequest request;
  request.mRemoveUser.emplace_back(std::move(uid));
  return asaRequestMutation(std::move(request));
}

rxcpp::observable<FakeVoid> Client::asaAddUserIdentifier(std::string existingUid, std::string newUid) {
  AsaMutationRequest request;
  request.mAddUserIdentifier.emplace_back(std::move(existingUid), std::move(newUid));
  return asaRequestMutation(std::move(request));
}

rxcpp::observable<FakeVoid> Client::asaRemoveUserIdentifier(std::string uid) {
  AsaMutationRequest request;
  request.mRemoveUserIdentifier.emplace_back(std::move(uid));
  return asaRequestMutation(std::move(request));
}

rxcpp::observable<FakeVoid> Client::asaCreateUserGroup(std::string name, UserGroupProperties properties) {
  AsaMutationRequest request;
  request.mCreateUserGroup.emplace_back(std::move(name), std::move(properties));
  return asaRequestMutation(std::move(request));
}

rxcpp::observable<FakeVoid> Client::asaModifyUserGroup(std::string name, UserGroupProperties properties) {
  AsaMutationRequest request;
  request.mModifyUserGroup.emplace_back(std::move(name), std::move(properties));
  return asaRequestMutation(std::move(request));
}

rxcpp::observable<FakeVoid> Client::asaRemoveUserGroup(std::string name) {
  AsaMutationRequest request;
  request.mRemoveUserGroup.emplace_back(std::move(name));
  return asaRequestMutation(std::move(request));
}

rxcpp::observable<FakeVoid> Client::asaAddUserToGroup(std::string uid, std::string group) {
  AsaMutationRequest request;
  request.mAddUserToGroup.emplace_back(std::move(uid), std::move(group));
  return asaRequestMutation(std::move(request));
}

rxcpp::observable<FakeVoid> Client::asaRemoveUserFromGroup(std::string uid, std::string group) {
  AsaMutationRequest request;
  request.mRemoveUserFromGroup.emplace_back(std::move(uid), std::move(group));
  return asaRequestMutation(std::move(request));
}

rxcpp::observable<AsaQueryResponse> Client::asaQuery(AsaQuery query) {
  return EnsureAuthServerConnected(clientAuthserver)->sendRequest<AsaQueryResponse>(sign(std::move(query)));
}

rxcpp::observable<std::string> Client::asaRequestToken(std::string subject,
                                                       std::string group,
                                                       Timestamp expirationTime) {
  return EnsureAuthServerConnected(clientAuthserver)
      ->sendRequest<AsaTokenResponse>(sign(AsaTokenRequest(subject, group, expirationTime))) // linebreak
      .map([](AsaTokenResponse response) { return response.mToken; });
}

} // namespace pep
