#pragma once

#include <pep/auth/Signed.hpp>
#include <pep/crypto/Timestamp.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>

namespace pep {

class AmaCreateColumn {
public:
  AmaCreateColumn() = default;
  AmaCreateColumn(std::string name) : name_(std::move(name)) { }
  std::string name_;
};

class AmaRemoveColumn {
public:
  AmaRemoveColumn() = default;
  AmaRemoveColumn(std::string name) : name_(std::move(name)) { }
  std::string name_;
};

class AmaCreateColumnGroup {
public:
  AmaCreateColumnGroup() = default;
  AmaCreateColumnGroup(std::string name) : name_(std::move(name)) { }
  std::string name_;
};

class AmaRemoveColumnGroup {
public:
  AmaRemoveColumnGroup() = default;
  AmaRemoveColumnGroup(std::string name) : name_(std::move(name)) { }
  std::string name_;
};

class AmaAddColumnToGroup {
public:
  AmaAddColumnToGroup() = default;
  AmaAddColumnToGroup(std::string column, std::string group)
    : column_(std::move(column)), columnGroup_(std::move(group)) { }
  std::string column_;
  std::string columnGroup_;
};

class AmaRemoveColumnFromGroup {
public:
  AmaRemoveColumnFromGroup() = default;
  AmaRemoveColumnFromGroup(std::string column, std::string group)
    : column_(std::move(column)), columnGroup_(std::move(group)) { }
  std::string column_;
  std::string columnGroup_;
};

class AmaCreateParticipantGroup {
public:
  AmaCreateParticipantGroup() = default;
  AmaCreateParticipantGroup(std::string name)
    : name_(std::move(name)) {}
  std::string name_;
};

class AmaRemoveParticipantGroup {
public:
  AmaRemoveParticipantGroup() = default;
  AmaRemoveParticipantGroup(std::string name)
    : name_(std::move(name)) {}
  std::string name_;
};

class AmaAddParticipantToGroup {
public:
  AmaAddParticipantToGroup() = default;
  AmaAddParticipantToGroup(std::string participantGroup, PolymorphicPseudonym participant)
    : participantGroup_(std::move(participantGroup)), participant_(participant) {}
  std::string participantGroup_;
  PolymorphicPseudonym participant_;
};

class AmaRemoveParticipantFromGroup {
public:
  AmaRemoveParticipantFromGroup() = default;
  AmaRemoveParticipantFromGroup(std::string participantGroup, PolymorphicPseudonym participant)
    : participantGroup_(std::move(participantGroup)), participant_(participant) {}
  std::string participantGroup_;
  PolymorphicPseudonym participant_;
};

class AmaCreateParticipantGroupAccessRule {
public:
  AmaCreateParticipantGroupAccessRule() = default;
  AmaCreateParticipantGroupAccessRule(std::string participantGroup,
    std::string userGroup, std::string mode)
    : participantGroup_(std::move(participantGroup)),
    userGroup_(std::move(userGroup)),
    mode_(std::move(mode)) { }
  std::string participantGroup_;
  std::string userGroup_;
  std::string mode_;
};

class AmaRemoveParticipantGroupAccessRule {
public:
  AmaRemoveParticipantGroupAccessRule() = default;
  AmaRemoveParticipantGroupAccessRule(std::string participantGroup,
    std::string userGroup, std::string mode)
    : participantGroup_(std::move(participantGroup)),
    userGroup_(std::move(userGroup)),
    mode_(std::move(mode)) { }
  std::string participantGroup_;
  std::string userGroup_;
  std::string mode_;
};

class AmaCreateColumnGroupAccessRule {
public:
  AmaCreateColumnGroupAccessRule() = default;
  AmaCreateColumnGroupAccessRule(std::string columnGroup,
    std::string userGroup, std::string mode)
    : columnGroup_(std::move(columnGroup)),
    userGroup_(std::move(userGroup)),
    mode_(std::move(mode)) { }
  std::string columnGroup_;
  std::string userGroup_;
  std::string mode_;
};

class AmaRemoveColumnGroupAccessRule {
public:
  AmaRemoveColumnGroupAccessRule() = default;
  AmaRemoveColumnGroupAccessRule(std::string columnGroup,
    std::string userGroup, std::string mode)
    : columnGroup_(std::move(columnGroup)),
    userGroup_(std::move(userGroup)),
    mode_(std::move(mode)) { }
  std::string columnGroup_;
  std::string userGroup_;
  std::string mode_;
};

class AmaMutationRequest {
public:
  std::vector<AmaCreateColumn> createColumn_;
  std::vector<AmaRemoveColumn> removeColumn_;
  std::vector<AmaCreateColumnGroup> createColumnGroup_;
  std::vector<AmaRemoveColumnGroup> removeColumnGroup_;
  std::vector<AmaAddColumnToGroup> addColumnToGroup_;
  std::vector<AmaRemoveColumnFromGroup> removeColumnFromGroup_;

  std::vector<AmaCreateParticipantGroup> createParticipantGroup_;
  std::vector<AmaRemoveParticipantGroup> removeParticipantGroup_;
  std::vector<AmaAddParticipantToGroup> addParticipantToGroup_;
  std::vector<AmaRemoveParticipantFromGroup> removeParticipantFromGroup_;

  std::vector<AmaCreateColumnGroupAccessRule> createColumnGroupAccessRule_;
  std::vector<AmaRemoveColumnGroupAccessRule> removeColumnGroupAccessRule_;
  std::vector<AmaCreateParticipantGroupAccessRule> createParticipantGroupAccessRule_;
  std::vector<AmaRemoveParticipantGroupAccessRule> removeParticipantGroupAccessRule_;

  bool forceColumnGroupRemoval_{false};
  bool forceParticipantGroupRemoval_{false};

  bool hasDataAdminOperation() const;
  bool hasAccessAdminOperation() const;
};

class AmaMutationResponse {
};

class AmaQuery {
public:
  // Use nullopt for current server time, such that a wrong client time does not influence query
  std::optional<Timestamp> at_{};
  std::string columnFilter_{};
  std::string columnGroupFilter_{};
  std::string participantGroupFilter_{};
  std::string userGroupFilter_{};
  std::string columnGroupModeFilter_{};
  std::string participantGroupModeFilter_{};
};

class AmaQRColumn {
public:
  AmaQRColumn() = default;
  AmaQRColumn(std::string name)
    : name_(std::move(name)) { }

  std::string name_;
};

class AmaQRColumnGroup {
public:
  AmaQRColumnGroup() = default;
  AmaQRColumnGroup(std::string name, std::vector<std::string> columns)
    : name_(std::move(name)), columns_(std::move(columns)) { }

  std::string name_;
  std::vector<std::string> columns_;

  /*
  *\brief Given a source AmaQRColumnGroup and a byte size capacity, fill a destination AmaQRColumnGroup with the name and items of the source until the capacity is reached.
  * The size is calculated by iteratively adding the lengths of all variables within the source columngroup (i.e. its name and the names of the columns) with a padding added
  * for each of those variables. The default of this padding is 2, based on the protobuf encoding described here: https://protobuf.dev/programming-guides/encoding/#length-types.
  * When this number is about to exceed the capacity, filling the destination will stop. The resulting size of the destination is returned.
  * An offset can be set to start filling from that index in the source.
  * If the capacity is smaller than the name of the source AmaQRColumnGroup, a 0 will be returned indicating that no copying has occurred.
  *
  * \param dest: destination AmaQRColumnGroup
  * \param source: source AmaQRColumnGroup
  * \param cap: The max capacity of the destination AmaQRColumnGroup in bytes
  * \param offset: The source index from which to start filling. Must not exceed the size of the source.
  * \param padding: Padding granting room for protobuf serialization.
  * \return The size of the destination AmaQRColumnGroup in bytes.
  */
  static size_t FillToProtobufSerializationCapacity(AmaQRColumnGroup& dest, const AmaQRColumnGroup& source, const size_t& cap, const size_t& offset = 0, const size_t& padding = 2);
};

class AmaQRColumnGroupAccessRule {
public:
  AmaQRColumnGroupAccessRule() = default;
  AmaQRColumnGroupAccessRule(std::string columnGroup,
    std::string accessGroup, std::string mode)
    : columnGroup_(std::move(columnGroup)),
    accessGroup_(std::move(accessGroup)),
    mode_(std::move(mode)) { }

  std::string columnGroup_;
  std::string accessGroup_;
  std::string mode_;
};

class AmaQRParticipantGroup {
public:
  AmaQRParticipantGroup() = default;
  AmaQRParticipantGroup(std::string name)
    : name_(std::move(name)) { }

  std::string name_;
};

class AmaQRParticipantGroupAccessRule {
public:
  AmaQRParticipantGroupAccessRule() = default;
  AmaQRParticipantGroupAccessRule(std::string participantGroup,
    std::string userGroup, std::string mode)
    : participantGroup_(std::move(participantGroup)),
    userGroup_(std::move(userGroup)),
    mode_(std::move(mode)) { }

  std::string participantGroup_;
  std::string userGroup_;
  std::string mode_;
};

class AmaQueryResponse {
public:
  std::vector<AmaQRColumn> columns_;
  std::vector<AmaQRColumnGroup> columnGroups_;
  std::vector<AmaQRColumnGroupAccessRule> columnGroupAccessRules_;
  std::vector<AmaQRParticipantGroup> participantGroups_;
  std::vector<AmaQRParticipantGroupAccessRule> participantGroupAccessRules_;
};

using SignedAmaMutationRequest = Signed<AmaMutationRequest>;
using SignedAmaQuery = Signed<AmaQuery>;

}
