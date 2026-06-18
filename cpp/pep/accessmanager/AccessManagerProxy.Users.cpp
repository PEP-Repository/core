#include <pep/accessmanager/AccessManagerProxy.hpp>
#include <pep/accessmanager/UserSerializers.hpp>
#include <pep/messaging/ResponseToVoid.hpp>

namespace pep {

rxcpp::observable<FakeVoid> AccessManagerProxy::requestUserMutation(UserMutationRequest request) const {
  return this->sendRequest<UserMutationResponse>(this->sign(std::move(request)))
    .op(messaging::ResponseToVoid());
}


rxcpp::observable<FakeVoid> AccessManagerProxy::createUser(std::string uid) const {
  UserMutationRequest request;
  request.createUser_.emplace_back(std::move(uid));
  return requestUserMutation(std::move(request));
}

rxcpp::observable<FakeVoid> AccessManagerProxy::removeUser(std::string uid) const {
  UserMutationRequest request;
  request.removeUser_.emplace_back(std::move(uid));
  return requestUserMutation(std::move(request));
}

rxcpp::observable<FakeVoid> AccessManagerProxy::addUserIdentifier(std::string existingUid, std::string newUid, bool isPrimaryId, bool isDisplayId) const {
  UserMutationRequest request;
  request.addUserIdentifier_.emplace_back(std::move(existingUid), std::move(newUid), isPrimaryId, isDisplayId);
  return requestUserMutation(std::move(request));
}

rxcpp::observable<FakeVoid> AccessManagerProxy::removeUserIdentifier(std::string uid) const {
  UserMutationRequest request;
  request.removeUserIdentifier_.emplace_back(std::move(uid));
  return requestUserMutation(std::move(request));
}

rxcpp::observable<FakeVoid> AccessManagerProxy::setUserPrimaryId(std::string uid) const {
  UserMutationRequest request;
  request.setPrimaryId_.emplace_back(std::move(uid));
  return requestUserMutation(std::move(request));
}

rxcpp::observable<FakeVoid> AccessManagerProxy::unsetUserPrimaryId(std::string uid) const {
  UserMutationRequest request;
  request.unsetPrimaryId_.emplace_back(std::move(uid));
  return requestUserMutation(std::move(request));
}

rxcpp::observable<FakeVoid> AccessManagerProxy::setUserDisplayId(std::string uid) const {
  UserMutationRequest request;
  request.setDisplayId_.emplace_back(std::move(uid));
  return requestUserMutation(std::move(request));
}

rxcpp::observable<FakeVoid> AccessManagerProxy::createUserGroup(UserGroup userGroup) const {
  UserMutationRequest request;
  request.createUserGroup_.emplace_back(std::move(userGroup));
  return requestUserMutation(std::move(request));
}

rxcpp::observable<FakeVoid> AccessManagerProxy::modifyUserGroup(UserGroup userGroup) const {
  UserMutationRequest request;
  request.modifyUserGroup_.emplace_back(std::move(userGroup));
  return requestUserMutation(std::move(request));
}

rxcpp::observable<FakeVoid> AccessManagerProxy::removeUserGroup(std::string name) const {
  UserMutationRequest request;
  request.removeUserGroup_.emplace_back(std::move(name));
  return requestUserMutation(std::move(request));
}

rxcpp::observable<FakeVoid> AccessManagerProxy::addUserToGroup(std::string uid, std::string group, std::optional<Timestamp> expiration) const {
  UserMutationRequest request;
  request.addUserToGroup_.emplace_back(std::move(uid), std::move(group), expiration);
  return requestUserMutation(std::move(request));
}

rxcpp::observable<FakeVoid> AccessManagerProxy::removeUserFromGroup(std::string uid, std::string group, bool blockTokens) const {
  UserMutationRequest request;
  request.removeUserFromGroup_.emplace_back(std::move(uid), std::move(group), blockTokens);
  return requestUserMutation(std::move(request));
}

rxcpp::observable<FakeVoid> AccessManagerProxy::updateExpiration(std::string uid, std::string group, std::optional<Timestamp> expiration) const {
  UserMutationRequest request;
  request.updateExpiration_.emplace_back(std::move(uid), std::move(group), expiration);
  return requestUserMutation(std::move(request));
}

rxcpp::observable<UserQueryResponse> AccessManagerProxy::userQuery(UserQuery query) const {
  return this->sendRequest<UserQueryResponse>(this->sign(std::move(query)))
    .op(RxGetOne());
}

} // namespace pep
