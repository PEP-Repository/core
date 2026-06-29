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
    auto found = std::find_if(destination.begin(), destination.end(), [sourceGroup](const AmaQRColumnGroup& destGroup) {return destGroup.name == sourceGroup.name; });
    if (found != destination.end()) {
      // The group already exists in the destination. Add the columns_ of the sourceGroup to this destinationGroup.
      AppendVector<std::string>(found->columns, sourceGroup.columns);
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
  request.createColumn.emplace_back(std::move(name));
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaRemoveColumn(std::string name) const {
  AmaMutationRequest request;
  request.removeColumn.emplace_back(std::move(name));
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaCreateColumnGroup(std::string name) const {
  AmaMutationRequest request;
  request.createColumnGroup.emplace_back(std::move(name));
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaRemoveColumnGroup(std::string name, bool force) const {
  AmaMutationRequest request;
  request.removeColumnGroup.emplace_back(std::move(name));
  request.forceColumnGroupRemoval = force;
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaAddColumnToGroup(std::string column, std::string group) const {
  AmaMutationRequest request;
  request.addColumnToGroup.emplace_back(std::move(column), std::move(group));
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaRemoveColumnFromGroup(std::string column, std::string group) const {
  AmaMutationRequest request;
  request.removeColumnFromGroup.emplace_back(std::move(column), std::move(group));
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaCreateParticipantGroup(std::string name) const {
  AmaMutationRequest request;
  request.createParticipantGroup.emplace_back(std::move(name));
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaRemoveParticipantGroup(std::string name, bool force) const {
  AmaMutationRequest request;
  request.removeParticipantGroup.emplace_back(std::move(name));
  request.forceParticipantGroupRemoval = force;
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaAddParticipantToGroup(std::string group, const PolymorphicPseudonym& participant) const {
  AmaMutationRequest request;
  request.addParticipantToGroup.emplace_back(std::move(group), participant);
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaRemoveParticipantsFromGroup(const std::string& group, const std::vector<PolymorphicPseudonym>& participants) const {
  AmaMutationRequest request;
  request.removeParticipantFromGroup.reserve(participants.size());
  std::transform(participants.begin(), participants.end(), std::back_inserter(request.removeParticipantFromGroup), [&group](const PolymorphicPseudonym& pp) {
    return AmaRemoveParticipantFromGroup(group, pp);
    });
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaRemoveParticipantFromGroup(std::string group, const PolymorphicPseudonym& participant) const {
  return this->amaRemoveParticipantsFromGroup(group, std::vector<PolymorphicPseudonym>{participant});
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaCreateColumnGroupAccessRule(std::string columnGroup,
    std::string accessGroup, std::string mode) const {
  AmaMutationRequest request;
  request.createColumnGroupAccessRule.emplace_back(std::move(columnGroup),
        std::move(accessGroup), std::move(mode));
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaRemoveColumnGroupAccessRule(std::string columnGroup,
    std::string accessGroup, std::string mode) const {
  AmaMutationRequest request;
  request.removeColumnGroupAccessRule.emplace_back(std::move(columnGroup),
        std::move(accessGroup), std::move(mode));
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaCreateGroupAccessRule(std::string group,
    std::string accessGroup, std::string mode) const {
  AmaMutationRequest request;
  request.createParticipantGroupAccessRule.emplace_back(std::move(group),
        std::move(accessGroup), std::move(mode));
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<FakeVoid>
AccessManagerProxy::amaRemoveGroupAccessRule(std::string group,
    std::string accessGroup, std::string mode) const {
  AmaMutationRequest request;
  request.removeParticipantGroupAccessRule.emplace_back(std::move(group),
        std::move(accessGroup), std::move(mode));
  return requestAmaMutation(std::move(request));
}

rxcpp::observable<AmaQueryResponse>
AccessManagerProxy::amaQuery(AmaQuery query) const {
  return this->sendRequest<AmaQueryResponse>(this->sign(std::move(query)))
    .reduce( // Concatenate all parts into a single AmaQueryResponse instance, which will remain empty if we didn't receive (a partial) one from AM
      std::make_shared<AmaQueryResponse>(),
      [](std::shared_ptr<AmaQueryResponse> all, const AmaQueryResponse& part) {
        AppendVector(all->columns, part.columns);
        AppendAndSquashVector(all->columnGroups, part.columnGroups);
        AppendVector(all->columnGroupAccessRules, part.columnGroupAccessRules);
        AppendVector(all->participantGroups, part.participantGroups);
        AppendVector(all->participantGroupAccessRules, part.participantGroupAccessRules);
        return all;
      }
    )
    .map([](std::shared_ptr<AmaQueryResponse> response) {return *response; }); // Return a plain AmaQueryResponse instead of a shared_ptr
}


}
