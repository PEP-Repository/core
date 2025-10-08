#include <pep/core-client/CoreClient.hpp>
#include <pep/accessmanager/UserSerializers.hpp>

namespace pep {

rxcpp::observable<FakeVoid> CoreClient::requestUserMutation(UserMutationRequest request) {
  return this->getAccessManagerProxy()
      ->requestUserMutation(std::move(request)) // linebreak
      .map([](UserMutationResponse resp) { return FakeVoid{}; });
}

rxcpp::observable<FakeVoid> CoreClient::createUser(std::string uid) {
  UserMutationRequest request;
  request.mCreateUser.emplace_back(std::move(uid));
  return requestUserMutation(std::move(request));
}

rxcpp::observable<FakeVoid> CoreClient::removeUser(std::string uid) {
  UserMutationRequest request;
  request.mRemoveUser.emplace_back(std::move(uid));
  return requestUserMutation(std::move(request));
}

rxcpp::observable<FakeVoid> CoreClient::addUserIdentifier(std::string existingUid, std::string newUid) {
  UserMutationRequest request;
  request.mAddUserIdentifier.emplace_back(std::move(existingUid), std::move(newUid));
  return requestUserMutation(std::move(request));
}

rxcpp::observable<FakeVoid> CoreClient::removeUserIdentifier(std::string uid) {
  UserMutationRequest request;
  request.mRemoveUserIdentifier.emplace_back(std::move(uid));
  return requestUserMutation(std::move(request));
}

rxcpp::observable<FakeVoid> CoreClient::createUserGroup(UserGroup userGroup) {
  UserMutationRequest request;
  request.mCreateUserGroup.emplace_back(std::move(userGroup));
  return requestUserMutation(std::move(request));
}

rxcpp::observable<FakeVoid> CoreClient::modifyUserGroup(UserGroup userGroup) {
  UserMutationRequest request;
  request.mModifyUserGroup.emplace_back(std::move(userGroup));
  return requestUserMutation(std::move(request));
}

rxcpp::observable<FakeVoid> CoreClient::removeUserGroup(std::string name) {
  UserMutationRequest request;
  request.mRemoveUserGroup.emplace_back(std::move(name));
  return requestUserMutation(std::move(request));
}

rxcpp::observable<FakeVoid> CoreClient::addUserToGroup(std::string uid, std::string group) {
  UserMutationRequest request;
  request.mAddUserToGroup.emplace_back(std::move(uid), std::move(group));
  return requestUserMutation(std::move(request));
}

rxcpp::observable<FakeVoid> CoreClient::removeUserFromGroup(std::string uid, std::string group, bool blockTokens) {
  UserMutationRequest request;
  request.mRemoveUserFromGroup.emplace_back(std::move(uid), std::move(group), blockTokens);
  return requestUserMutation(std::move(request));
}

rxcpp::observable<UserQueryResponse> CoreClient::userQuery(UserQuery query) {
  return this->getAccessManagerProxy()->requestUserQuery(std::move(query));
}

} // namespace pep
