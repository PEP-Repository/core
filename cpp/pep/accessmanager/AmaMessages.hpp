#pragma once

#include <pep/crypto/Signed.hpp>
#include <pep/crypto/Timestamp.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>

namespace pep {

class AmaCreateColumn {
public:
  AmaCreateColumn() = default;
  AmaCreateColumn(std::string name) : mName(std::move(name)) { }
  std::string mName;
};

class AmaRemoveColumn {
public:
  AmaRemoveColumn() = default;
  AmaRemoveColumn(std::string name) : mName(std::move(name)) { }
  std::string mName;
};

class AmaCreateColumnGroup {
public:
  AmaCreateColumnGroup() = default;
  AmaCreateColumnGroup(std::string name) : mName(std::move(name)) { }
  std::string mName;
};

class AmaRemoveColumnGroup {
public:
  AmaRemoveColumnGroup() = default;
  AmaRemoveColumnGroup(std::string name) : mName(std::move(name)) { }
  std::string mName;
};

class AmaAddColumnToGroup {
public:
  AmaAddColumnToGroup() = default;
  AmaAddColumnToGroup(std::string column, std::string group)
    : mColumn(std::move(column)), mColumnGroup(std::move(group)) { }
  std::string mColumn;
  std::string mColumnGroup;
};

class AmaRemoveColumnFromGroup {
public:
  AmaRemoveColumnFromGroup() = default;
  AmaRemoveColumnFromGroup(std::string column, std::string group)
    : mColumn(std::move(column)), mColumnGroup(std::move(group)) { }
  std::string mColumn;
  std::string mColumnGroup;
};

class AmaCreateParticipantGroup {
public:
  AmaCreateParticipantGroup() = default;
  AmaCreateParticipantGroup(std::string name)
    : mName(std::move(name)) {}
  std::string mName;
};

class AmaRemoveParticipantGroup {
public:
  AmaRemoveParticipantGroup() = default;
  AmaRemoveParticipantGroup(std::string name)
    : mName(std::move(name)) {}
  std::string mName;
};

class AmaAddParticipantToGroup {
public:
  AmaAddParticipantToGroup() = default;
  AmaAddParticipantToGroup(std::string participantGroup, PolymorphicPseudonym participant)
    : mParticipantGroup(std::move(participantGroup)), mParticipant(participant) {}
  std::string mParticipantGroup;
  PolymorphicPseudonym mParticipant;
};

class AmaRemoveParticipantFromGroup {
public:
  AmaRemoveParticipantFromGroup() = default;
  AmaRemoveParticipantFromGroup(std::string participantGroup, PolymorphicPseudonym participant)
    : mParticipantGroup(std::move(participantGroup)), mParticipant(participant) {}
  std::string mParticipantGroup;
  PolymorphicPseudonym mParticipant;
};

class AmaCreateParticipantGroupAccessRule {
public:
  AmaCreateParticipantGroupAccessRule() = default;
  AmaCreateParticipantGroupAccessRule(std::string participantGroup,
    std::string userGroup, std::string mode)
    : mParticipantGroup(std::move(participantGroup)),
    mUserGroup(std::move(userGroup)),
    mMode(std::move(mode)) { }
  std::string mParticipantGroup;
  std::string mUserGroup;
  std::string mMode;
};

class AmaRemoveParticipantGroupAccessRule {
public:
  AmaRemoveParticipantGroupAccessRule() = default;
  AmaRemoveParticipantGroupAccessRule(std::string participantGroup,
    std::string userGroup, std::string mode)
    : mParticipantGroup(std::move(participantGroup)),
    mUserGroup(std::move(userGroup)),
    mMode(std::move(mode)) { }
  std::string mParticipantGroup;
  std::string mUserGroup;
  std::string mMode;
};

class AmaCreateColumnGroupAccessRule {
public:
  AmaCreateColumnGroupAccessRule() = default;
  AmaCreateColumnGroupAccessRule(std::string columnGroup,
    std::string userGroup, std::string mode)
    : mColumnGroup(std::move(columnGroup)),
    mUserGroup(std::move(userGroup)),
    mMode(std::move(mode)) { }
  std::string mColumnGroup;
  std::string mUserGroup;
  std::string mMode;
};

class AmaRemoveColumnGroupAccessRule {
public:
  AmaRemoveColumnGroupAccessRule() = default;
  AmaRemoveColumnGroupAccessRule(std::string columnGroup,
    std::string userGroup, std::string mode)
    : mColumnGroup(std::move(columnGroup)),
    mUserGroup(std::move(userGroup)),
    mMode(std::move(mode)) { }
  std::string mColumnGroup;
  std::string mUserGroup;
  std::string mMode;
};

class AmaMutationRequest {
public:
  std::vector<AmaCreateColumn> mCreateColumn;
  std::vector<AmaRemoveColumn> mRemoveColumn;
  std::vector<AmaCreateColumnGroup> mCreateColumnGroup;
  std::vector<AmaRemoveColumnGroup> mRemoveColumnGroup;
  std::vector<AmaAddColumnToGroup> mAddColumnToGroup;
  std::vector<AmaRemoveColumnFromGroup> mRemoveColumnFromGroup;

  std::vector<AmaCreateParticipantGroup> mCreateParticipantGroup;
  std::vector<AmaRemoveParticipantGroup> mRemoveParticipantGroup;
  std::vector<AmaAddParticipantToGroup> mAddParticipantToGroup;
  std::vector<AmaRemoveParticipantFromGroup> mRemoveParticipantFromGroup;

  std::vector<AmaCreateColumnGroupAccessRule> mCreateColumnGroupAccessRule;
  std::vector<AmaRemoveColumnGroupAccessRule> mRemoveColumnGroupAccessRule;
  std::vector<AmaCreateParticipantGroupAccessRule> mCreateParticipantGroupAccessRule;
  std::vector<AmaRemoveParticipantGroupAccessRule> mRemoveParticipantGroupAccessRule;

  bool mForceColumnGroupRemoval{false};
  bool mForceParticipantGroupRemoval{false};

  bool hasDataAdminOperation() const;
  bool hasAccessAdminOperation() const;
};

class AmaMutationResponse {
};

class AmaQuery {
public:
  Timestamp mAt;
  std::string mColumnFilter{};
  std::string mColumnGroupFilter{};
  std::string mParticipantGroupFilter{};
  std::string mUserGroupFilter{};
  std::string mColumnGroupModeFilter{};
  std::string mParticipantGroupModeFilter{};
};

class AmaQRColumn {
public:
  AmaQRColumn() = default;
  AmaQRColumn(std::string name)
    : mName(std::move(name)) { }

  std::string mName;
};

class AmaQRColumnGroup {
public:
  AmaQRColumnGroup() = default;
  AmaQRColumnGroup(std::string name, std::vector<std::string> columns)
    : mName(std::move(name)), mColumns(std::move(columns)) { }

  std::string mName;
  std::vector<std::string> mColumns;

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
    : mColumnGroup(std::move(columnGroup)),
    mAccessGroup(std::move(accessGroup)),
    mMode(std::move(mode)) { }

  std::string mColumnGroup;
  std::string mAccessGroup;
  std::string mMode;
};

class AmaQRParticipantGroup {
public:
  AmaQRParticipantGroup() = default;
  AmaQRParticipantGroup(std::string name)
    : mName(std::move(name)) { }

  std::string mName;
};

class AmaQRParticipantGroupAccessRule {
public:
  AmaQRParticipantGroupAccessRule() = default;
  AmaQRParticipantGroupAccessRule(std::string participantGroup,
    std::string userGroup, std::string mode)
    : mParticipantGroup(std::move(participantGroup)),
    mUserGroup(std::move(userGroup)),
    mMode(std::move(mode)) { }

  std::string mParticipantGroup;
  std::string mUserGroup;
  std::string mMode;
};

class AmaQueryResponse {
public:
  std::vector<AmaQRColumn> mColumns;
  std::vector<AmaQRColumnGroup> mColumnGroups;
  std::vector<AmaQRColumnGroupAccessRule> mColumnGroupAccessRules;
  std::vector<AmaQRParticipantGroup> mParticipantGroups;
  std::vector<AmaQRParticipantGroupAccessRule> mParticipantGroupAccessRules;
};

using SignedAmaMutationRequest = Signed<AmaMutationRequest>;
using SignedAmaQuery = Signed<AmaQuery>;

}
