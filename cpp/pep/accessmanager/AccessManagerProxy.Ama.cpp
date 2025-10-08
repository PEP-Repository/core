#include <pep/accessmanager/AccessManagerProxy.hpp>
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

rxcpp::observable<FakeVoid> AccessManagerProxy::requestAmaMutation(AmaMutationRequest request) const {
  return this->sendRequest<AmaMutationResponse>(this->sign(std::move(request)))
    .op(messaging::ResponseToVoid());
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaCreateColumn(std::string name) const {
  AmaMutationRequest request;
  request.mCreateColumn.emplace_back(std::move(name));
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaRemoveColumn(std::string name) const {
  AmaMutationRequest request;
  request.mRemoveColumn.emplace_back(std::move(name));
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaCreateColumnGroup(std::string name) const {
  AmaMutationRequest request;
  request.mCreateColumnGroup.emplace_back(std::move(name));
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaRemoveColumnGroup(std::string name, bool force) const {
  AmaMutationRequest request;
  request.mRemoveColumnGroup.emplace_back(std::move(name));
  request.mForceColumnGroupRemoval = force;
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaAddColumnToGroup(std::string column, std::string group) const {
  AmaMutationRequest request;
  request.mAddColumnToGroup.emplace_back(std::move(column), std::move(group));
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaRemoveColumnFromGroup(std::string column, std::string group) const {
  AmaMutationRequest request;
  request.mRemoveColumnFromGroup.emplace_back(std::move(column), std::move(group));
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaCreateParticipantGroup(std::string name) const {
  AmaMutationRequest request;
  request.mCreateParticipantGroup.emplace_back(std::move(name));
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaRemoveParticipantGroup(std::string name, bool force) const {
  AmaMutationRequest request;
  request.mRemoveParticipantGroup.emplace_back(std::move(name));
  request.mForceParticipantGroupRemoval = force;
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaAddParticipantToGroup(std::string group, const PolymorphicPseudonym& participant) const {
  AmaMutationRequest request;
  request.mAddParticipantToGroup.emplace_back(std::move(group), participant);
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaRemoveParticipantFromGroup(std::string group, const PolymorphicPseudonym& participant) const {
  AmaMutationRequest request;
  request.mRemoveParticipantFromGroup.emplace_back(std::move(group), participant);
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaCreateColumnGroupAccessRule(std::string columnGroup,
    std::string accessGroup, std::string mode) const {
  AmaMutationRequest request;
  request.mCreateColumnGroupAccessRule.emplace_back(std::move(columnGroup),
        std::move(accessGroup), std::move(mode));
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaRemoveColumnGroupAccessRule(std::string columnGroup,
    std::string accessGroup, std::string mode) const {
  AmaMutationRequest request;
  request.mRemoveColumnGroupAccessRule.emplace_back(std::move(columnGroup),
        std::move(accessGroup), std::move(mode));
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaCreateGroupAccessRule(std::string group,
    std::string accessGroup, std::string mode) const {
  AmaMutationRequest request;
  request.mCreateParticipantGroupAccessRule.emplace_back(std::move(group),
        std::move(accessGroup), std::move(mode));
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaRemoveGroupAccessRule(std::string group,
    std::string accessGroup, std::string mode) const {
  AmaMutationRequest request;
  request.mRemoveParticipantGroupAccessRule.emplace_back(std::move(group),
        std::move(accessGroup), std::move(mode));
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<AmaQueryResponse>
AccessManagerProxy::amaQuery(AmaQuery query) const {
  return this->sendRequest<AmaQueryResponse>(this->sign(std::move(query)))
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
