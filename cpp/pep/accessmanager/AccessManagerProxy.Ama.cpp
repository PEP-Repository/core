#include <pep/accessmanager/AccessManagerProxy.hpp>
#include <pep/accessmanager/AmaSerializers.hpp>
#include <pep/messaging/ResponseToVoid.hpp>
#include <ranges>
#include <utility>

namespace pep {

namespace {

void AppendAndSquashVector(std::vector<AmaQRColumnGroup>& destination, const std::vector<AmaQRColumnGroup>& source) {
  for (auto& sourceGroup : source) {
    auto found = std::ranges::find(destination, sourceGroup.name, &AmaQRColumnGroup::name);
    if (found != destination.end()) {
      // The group already exists in the destination. Add the columns_ of the sourceGroup to this destinationGroup.
      found->columns.append_range(sourceGroup.columns);
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
  request.removeParticipantFromGroup = participants
    | std::views::transform([&group](const PolymorphicPseudonym& pp) {
      return AmaRemoveParticipantFromGroup(group, pp);
      })
    | std::ranges::to<std::vector>();
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
        all->columns.append_range(part.columns);
        AppendAndSquashVector(all->columnGroups, part.columnGroups);
        all->columnGroupAccessRules.append_range(part.columnGroupAccessRules);
        all->participantGroups.append_range(part.participantGroups);
        all->participantGroupAccessRules.append_range(part.participantGroupAccessRules);
        return all;
      }
    )
    .map([](std::shared_ptr<AmaQueryResponse> response) {return *response; }); // Return a plain AmaQueryResponse instead of a shared_ptr
}


}
