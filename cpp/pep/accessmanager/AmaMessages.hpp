#pragma once

#include <pep/auth/Signed.hpp>
#include <pep/crypto/Timestamp.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>

namespace pep {

struct AmaCreateColumn {
  std::string name;
};

struct AmaRemoveColumn {
  std::string name;
};

struct AmaCreateColumnGroup {
  std::string name;
};

struct AmaRemoveColumnGroup {
  std::string name;
};

struct AmaAddColumnToGroup {
  std::string column;
  std::string columnGroup;
};

struct AmaRemoveColumnFromGroup {
  std::string column;
  std::string columnGroup;
};

struct AmaCreateParticipantGroup {
  std::string name;
};

struct AmaRemoveParticipantGroup {
  std::string name;
};

struct AmaAddParticipantToGroup {
  std::string participantGroup;
  PolymorphicPseudonym participant;
};

struct AmaRemoveParticipantFromGroup {
  std::string participantGroup;
  PolymorphicPseudonym participant;
};

struct AmaCreateParticipantGroupAccessRule {
  std::string participantGroup;
  std::string userGroup;
  std::string mode;
};

struct AmaRemoveParticipantGroupAccessRule {
  std::string participantGroup;
  std::string userGroup;
  std::string mode;
};

struct AmaCreateColumnGroupAccessRule {
  std::string columnGroup;
  std::string userGroup;
  std::string mode;
};

struct AmaRemoveColumnGroupAccessRule {
  std::string columnGroup;
  std::string userGroup;
  std::string mode;
};

struct AmaMutationRequest {
  std::vector<AmaCreateColumn> createColumn;
  std::vector<AmaRemoveColumn> removeColumn;
  std::vector<AmaCreateColumnGroup> createColumnGroup;
  std::vector<AmaRemoveColumnGroup> removeColumnGroup;
  std::vector<AmaAddColumnToGroup> addColumnToGroup;
  std::vector<AmaRemoveColumnFromGroup> removeColumnFromGroup;

  std::vector<AmaCreateParticipantGroup> createParticipantGroup;
  std::vector<AmaRemoveParticipantGroup> removeParticipantGroup;
  std::vector<AmaAddParticipantToGroup> addParticipantToGroup;
  std::vector<AmaRemoveParticipantFromGroup> removeParticipantFromGroup;

  std::vector<AmaCreateColumnGroupAccessRule> createColumnGroupAccessRule;
  std::vector<AmaRemoveColumnGroupAccessRule> removeColumnGroupAccessRule;
  std::vector<AmaCreateParticipantGroupAccessRule> createParticipantGroupAccessRule;
  std::vector<AmaRemoveParticipantGroupAccessRule> removeParticipantGroupAccessRule;

  bool forceColumnGroupRemoval{false};
  bool forceParticipantGroupRemoval{false};

  bool hasDataAdminOperation() const;
  bool hasAccessAdminOperation() const;
};

struct AmaMutationResponse {};

struct AmaQuery {
  // Use nullopt for current server time, such that a wrong client time does not influence query
  std::optional<Timestamp> at{};
  std::string columnFilter{};
  std::string columnGroupFilter{};
  std::string participantGroupFilter{};
  std::string userGroupFilter{};
  std::string columnGroupModeFilter{};
  std::string participantGroupModeFilter{};
};

struct AmaQRColumn {
  std::string name;
};

struct AmaQRColumnGroup {
  std::string name;
  std::vector<std::string> columns;

  /*
  *\brief Given a source AmaQRColumnGroup and a byte size capacity, fill a destination AmaQRColumnGroup with the name and items of the source until the capacity is reached.
  * The size is calculated by iteratively adding the lengths of all variables within the source columngroup (i.e. its name and the names of the columns) with a padding added
  * for each of those variables. The default of this padding is 2, based on the protobuf encoding described here: https://protobuf.dev/programming-guides/encoding/#length-types.
  * When this number is about to exceed the capacity, filling the destination will stop. The resulting size of the destination is returned.
  * An offset can be set to start filling from that index in the source.
  * If the capacity is smaller than the name of the source AmaQRColumnGroup, a 0 will be returned indicating that no copying has occurred.
  *
  * \param dest: destination AmaQRColumnGroup
  * \param cap: The max capacity of the destination AmaQRColumnGroup in bytes
  * \param source: source AmaQRColumnGroup
  * \param offset: The source index from which to start filling. Must not exceed the size of the source.
  * \param padding: Padding granting room for protobuf serialization.
  * \return The size of the destination AmaQRColumnGroup in bytes.
  */
  static size_t FillToProtobufSerializationCapacity(AmaQRColumnGroup& dest, size_t cap, const AmaQRColumnGroup& source, size_t offset = 0, size_t padding = 2);
};

struct AmaQRColumnGroupAccessRule {
  std::string columnGroup;
  std::string accessGroup;
  std::string mode;
};

struct AmaQRParticipantGroup {
  std::string name;
};

struct AmaQRParticipantGroupAccessRule {
public:
  std::string participantGroup;
  std::string userGroup;
  std::string mode;
};

struct AmaQueryResponse {
  std::vector<AmaQRColumn> columns;
  std::vector<AmaQRColumnGroup> columnGroups;
  std::vector<AmaQRColumnGroupAccessRule> columnGroupAccessRules;
  std::vector<AmaQRParticipantGroup> participantGroups;
  std::vector<AmaQRParticipantGroupAccessRule> participantGroupAccessRules;
};

using SignedAmaMutationRequest = Signed<AmaMutationRequest>;
using SignedAmaQuery = Signed<AmaQuery>;

}
