#include <pep/core-client/CoreClient.hpp>
#include <pep/accessmanager/AmaSerializers.hpp>
#include <pep/messaging/ResponseToVoid.hpp>
#include <utility>

namespace pep {

namespace {

template <typename T>
void AppendVector(std::vector<T>& destination, const std::vector<T>& source) {
  destination.insert(destination.end(), source.begin(), source.end());
}

void AppendAndSquashVector(std::vector<AmaQRColumnGroup>& destination, const std::vector<AmaQRColumnGroup>& source) {
  for (auto& sourceGroup : source) {
    auto found = std::find_if(destination.begin(), destination.end(), [sourceGroup](const AmaQRColumnGroup& destGroup) {return destGroup.mName == sourceGroup.mName; });
    if (found != destination.end()) {
      // The group already exists in the destination. Add the mColumns of the sourceGroup to this destinationGroup.
      AppendVector<std::string>(found->mColumns, sourceGroup.mColumns);
    }
    else {
      destination.push_back(sourceGroup);
    }
  }
}
}

rxcpp::observable<FakeVoid> CoreClient::amaRequestMutation(AmaMutationRequest request) {
  return accessManagerProxy->requestAmaMutation(std::move(request))
    .op(messaging::ResponseToVoid());
}

rxcpp::observable<FakeVoid>
CoreClient::amaCreateColumn(std::string name) {
  AmaMutationRequest request;
  request.mCreateColumn.emplace_back(std::move(name));
  return amaRequestMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
CoreClient::amaRemoveColumn(std::string name) {
  AmaMutationRequest request;
  request.mRemoveColumn.emplace_back(std::move(name));
  return amaRequestMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
CoreClient::amaCreateColumnGroup(std::string name) {
  AmaMutationRequest request;
  request.mCreateColumnGroup.emplace_back(std::move(name));
  return amaRequestMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
CoreClient::amaRemoveColumnGroup(std::string name, bool force) {
  AmaMutationRequest request;
  request.mRemoveColumnGroup.emplace_back(std::move(name));
  request.mForceColumnGroupRemoval = force;
  return amaRequestMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
CoreClient::amaAddColumnToGroup(std::string column, std::string group) {
  AmaMutationRequest request;
  request.mAddColumnToGroup.emplace_back(std::move(column), std::move(group));
  return amaRequestMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
CoreClient::amaRemoveColumnFromGroup(std::string column, std::string group) {
  AmaMutationRequest request;
  request.mRemoveColumnFromGroup.emplace_back(std::move(column), std::move(group));
  return amaRequestMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
CoreClient::amaCreateParticipantGroup(std::string name) {
  AmaMutationRequest request;
  request.mCreateParticipantGroup.emplace_back(std::move(name));
  return amaRequestMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
CoreClient::amaRemoveParticipantGroup(std::string name, bool force) {
  AmaMutationRequest request;
  request.mRemoveParticipantGroup.emplace_back(std::move(name));
  request.mForceParticipantGroupRemoval = force;
  return amaRequestMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
CoreClient::amaAddParticipantToGroup(std::string group, const PolymorphicPseudonym& participant) {
  AmaMutationRequest request;
  request.mAddParticipantToGroup.emplace_back(std::move(group), participant);
  return amaRequestMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
CoreClient::amaRemoveParticipantFromGroup(std::string group, const PolymorphicPseudonym& participant) {
  AmaMutationRequest request;
  request.mRemoveParticipantFromGroup.emplace_back(std::move(group), participant);
  return amaRequestMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
CoreClient::amaCreateColumnGroupAccessRule(std::string columnGroup,
    std::string accessGroup, std::string mode) {
  AmaMutationRequest request;
  request.mCreateColumnGroupAccessRule.emplace_back(std::move(columnGroup),
        std::move(accessGroup), std::move(mode));
  return amaRequestMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
CoreClient::amaRemoveColumnGroupAccessRule(std::string columnGroup,
    std::string accessGroup, std::string mode) {
  AmaMutationRequest request;
  request.mRemoveColumnGroupAccessRule.emplace_back(std::move(columnGroup),
        std::move(accessGroup), std::move(mode));
  return amaRequestMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
CoreClient::amaCreateGroupAccessRule(std::string group,
    std::string accessGroup, std::string mode) {
  AmaMutationRequest request;
  request.mCreateParticipantGroupAccessRule.emplace_back(std::move(group),
        std::move(accessGroup), std::move(mode));
  return amaRequestMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
CoreClient::amaRemoveGroupAccessRule(std::string group,
    std::string accessGroup, std::string mode) {
  AmaMutationRequest request;
  request.mRemoveParticipantGroupAccessRule.emplace_back(std::move(group),
        std::move(accessGroup), std::move(mode));
  return amaRequestMutation(std::move(request));
}

rxcpp::observable<AmaQueryResponse>
CoreClient::amaQuery(AmaQuery query) {
  return accessManagerProxy->requestAmaQuery(std::move(query)) // Send the query to AM
    .op(pep::RxRequireNonEmpty()) // Ensure we don't return an AmaQueryResponse if we didn't receive one from AM
    .reduce( // Concatenate all parts into a single AmaQueryResponse instance
      std::make_shared<AmaQueryResponse>(),
      [](std::shared_ptr<AmaQueryResponse> all, const AmaQueryResponse& part) {
        AppendVector(all->mColumns, part.mColumns);
        AppendAndSquashVector(all->mColumnGroups, part.mColumnGroups);
        AppendVector(all->mColumnGroupAccessRules, part.mColumnGroupAccessRules);
        AppendVector(all->mParticipantGroups, part.mParticipantGroups);
        AppendVector(all->mParticipantGroupAccessRules, part.mParticipantGroupAccessRules);
        return all;
      }
    )
    .map([](std::shared_ptr<AmaQueryResponse> response) {return *response; }); // Return a plain AmaQueryResponse instead of a shared_ptr
}



}
