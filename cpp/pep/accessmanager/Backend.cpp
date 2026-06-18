#include <pep/accessmanager/Storage.hpp>
#include <pep/accessmanager/AccessManager.hpp>
#include <pep/keyserver/KeyServerMessages.hpp>
#include <pep/auth/UserGroup.hpp>
#include <pep/async/RxInstead.hpp>
#include <pep/utils/Random.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/CollectionUtils.hpp>

#include <boost/algorithm/string/join.hpp>


namespace pep {

namespace {

const std::string LogTag("AccessManager::Backend");

template <typename TValue>
void EnsureMapContains(std::unordered_map<std::string, TValue>& map, std::span<const std::string> keys) {
  for (const std::string& key : keys) {
    map.try_emplace(key);
  }
}

enum class AccessMode {
  Read,
  Write
};
void EnsureStructureMetadataAccess(AccessMode mode, StructureMetadataType subjectType, std::string_view userGroup) {
  if (subjectType == StructureMetadataType::User || subjectType == StructureMetadataType::UserGroup) {
    UserGroup::EnsureAccess({UserGroup::AccessAdministrator}, userGroup);
  }
  else {
    if (mode == AccessMode::Write) {
      UserGroup::EnsureAccess({UserGroup::DataAdministrator}, userGroup);
    }
  }
}

}

AccessManager::Backend::Backend(const std::filesystem::path& path, std::shared_ptr<GlobalConfiguration> globalConf)
  : Backend(std::make_shared<AccessManager::Backend::Storage>(path, globalConf)) {
}

/********** START AMA Operations For Requests **********/
void AccessManager::Backend::createColumnsForRequest(const AmaMutationRequest& amRequest) {
  for (auto& mutation : amRequest.createColumn_) {
    this->storage_->createColumn(mutation.name_);
    PEP_LOG(LogTag, Severity::Info) << "Created column " << Logging::Escape(mutation.name_);
  }
}
void AccessManager::Backend::removeColumnsForRequest(const AmaMutationRequest& amRequest) {
  for (auto& mutation : amRequest.removeColumn_) {
    this->storage_->removeColumn(mutation.name_);
    PEP_LOG(LogTag, Severity::Info) << "Removed column " << Logging::Escape(mutation.name_);
  }
}
void AccessManager::Backend::createColumnGroupsForRequest(const AmaMutationRequest& amRequest) {
  for (auto& mutation : amRequest.createColumnGroup_) {
    this->storage_->createColumnGroup(mutation.name_);
    PEP_LOG(LogTag, Severity::Info) << "Created columngroup " << Logging::Escape(mutation.name_);
  }
}
void AccessManager::Backend::removeColumnGroupsForRequest(const AmaMutationRequest& amRequest) {
  for (auto& mutation : amRequest.removeColumnGroup_) {
    this->storage_->removeColumnGroup(mutation.name_, amRequest.forceColumnGroupRemoval_);
    PEP_LOG(LogTag, Severity::Info) << "Removed columngroup " << Logging::Escape(mutation.name_);
  }
}
void AccessManager::Backend::addColumnsToGroupsForRequest(const AmaMutationRequest& amRequest) {
  for (auto& mutation : amRequest.addColumnToGroup_) {
    this->storage_->addColumnToGroup(mutation.column_, mutation.columnGroup_);
    PEP_LOG(LogTag, Severity::Info) << "Added column " << Logging::Escape(mutation.column_) << " to group "
                                               << Logging::Escape(mutation.columnGroup_);
  }
}
void AccessManager::Backend::removeColumnsFromGroupsForRequest(const AmaMutationRequest& amRequest) {
  for (auto& mutation : amRequest.removeColumnFromGroup_) {
    this->storage_->removeColumnFromGroup(mutation.column_, mutation.columnGroup_);
    PEP_LOG(LogTag, Severity::Info) << "Removed column " << Logging::Escape(mutation.column_)
                                               << " from group " << Logging::Escape(mutation.columnGroup_);
  }
}
void AccessManager::Backend::createParticipantGroupsForRequest(const AmaMutationRequest& amRequest) {
  for (auto& mutation : amRequest.createParticipantGroup_) {
    this->storage_->createParticipantGroup(mutation.name_);
    PEP_LOG(LogTag, Severity::Info)
        << "Created participant group via ref " << Logging::Escape(mutation.name_);
  }
}
void AccessManager::Backend::removeParticipantGroupsForRequest(const AmaMutationRequest& amRequest) {
  for (auto& mutation : amRequest.removeParticipantGroup_) {
    this->storage_->removeParticipantGroup(mutation.name_, amRequest.forceParticipantGroupRemoval_);
    PEP_LOG(LogTag, Severity::Info) << "Removed participant group " << Logging::Escape(mutation.name_);
  }
}
void AccessManager::Backend::createColumnGroupAccessRulesForRequest(const AmaMutationRequest& amRequest) {
  for (auto& mutation : amRequest.createColumnGroupAccessRule_) {
    this->storage_->createColumnGroupAccessRule(mutation.columnGroup_, mutation.userGroup_, mutation.mode_);
    PEP_LOG(LogTag, Severity::Info)
        << "Created column-group-access-rule: " << Logging::Escape(mutation.userGroup_) << " has access to mode "
        << Logging::Escape(mutation.mode_) << " for column group " << Logging::Escape(mutation.columnGroup_);
  }
}
void AccessManager::Backend::removeColumnGroupAccessRulesForRequest(const AmaMutationRequest& amRequest) {
  for (auto& mutation : amRequest.removeColumnGroupAccessRule_) {
    this->storage_->removeColumnGroupAccessRule(mutation.columnGroup_, mutation.userGroup_, mutation.mode_);
    PEP_LOG(LogTag, Severity::Info)
        << "Removed column-group-access-rule: " << Logging::Escape(mutation.userGroup_)
        << " no longer has access to mode " << Logging::Escape(mutation.mode_) << " for column group "
        << Logging::Escape(mutation.columnGroup_);
  }
}
void AccessManager::Backend::createParticipantGroupAccessRulesForRequest(const AmaMutationRequest& amRequest) {
  for (auto& mutation : amRequest.createParticipantGroupAccessRule_) {
    this->storage_->createParticipantGroupAccessRule(mutation.participantGroup_, mutation.userGroup_, mutation.mode_);
    PEP_LOG(LogTag, Severity::Info)
        << "Created group-access-rule: " << Logging::Escape(mutation.userGroup_) << " has access to mode "
        << Logging::Escape(mutation.mode_) << " for group " << Logging::Escape(mutation.participantGroup_);
  }
}
void AccessManager::Backend::removeParticipantGroupAccessRulesForRequest(const AmaMutationRequest& amRequest) {
  for (auto& mutation : amRequest.removeParticipantGroupAccessRule_) {
    this->storage_->removeParticipantGroupAccessRule(mutation.participantGroup_, mutation.userGroup_, mutation.mode_);
    PEP_LOG(LogTag, Severity::Info)
        << "Removed group-access-rule: " << Logging::Escape(mutation.userGroup_) << " no longer has access to mode "
        << Logging::Escape(mutation.mode_) << " for group " << Logging::Escape(mutation.participantGroup_);
  }
}

void AccessManager::Backend::performMutationsForRequest(const AmaMutationRequest& request, const std::string& userGroup) {

  // Check for required access groups for some operations.
  if (request.hasDataAdminOperation()) UserGroup::EnsureAccess({UserGroup::DataAdministrator}, userGroup);
  if (request.hasAccessAdminOperation()) UserGroup::EnsureAccess({UserGroup::AccessAdministrator}, userGroup);

  // Execute mutations. When adding actions to the list below, don't forget to check access for it in
  // hasDataAdminOperation() and/or hasAccessAdminOperation() above!
  this->createColumnsForRequest(request);
  this->removeColumnsForRequest(request);
  this->createColumnGroupsForRequest(request);
  this->removeColumnGroupsForRequest(request);
  this->addColumnsToGroupsForRequest(request);
  this->removeColumnsFromGroupsForRequest(request);
  this->createParticipantGroupsForRequest(request);
  this->removeParticipantGroupsForRequest(request);
  this->createColumnGroupAccessRulesForRequest(request);
  this->removeColumnGroupAccessRulesForRequest(request);
  this->createParticipantGroupAccessRulesForRequest(request);
  this->removeParticipantGroupAccessRulesForRequest(request);
}

rxcpp::observable<UserMutationResponse> AccessManager::Backend::performUserMutationsForRequest(
  const UserMutationRequest& request, const std::string& userGroup) {
  // Check access
  UserGroup::EnsureAccess({UserGroup::AccessAdministrator}, userGroup);

  // Execute mutations
  std::vector<rxcpp::observable<rxcpp::observable<std::string>>> observables;
  for (auto& x : request.createUser_) {
    storage_->createUser(x.uid_);
    PEP_LOG(LogTag, Severity::Info) << "Created user " << Logging::Escape(x.uid_);
  }
  for (auto& x : request.removeUser_) {
    storage_->removeUser(x.uid_);
    PEP_LOG(LogTag, Severity::Info) << "Removed user " << Logging::Escape(x.uid_);
  }
  for (auto& x : request.addUserIdentifier_) {
    const auto flags =
        FlagsIf(UserIdFlags::IsDisplayId, x.isDisplayId_) |
        FlagsIf(UserIdFlags::IsPrimaryId, x.isPrimaryId_);
    storage_->addIdentifierForUser(x.existingUid_, x.newUid_, flags);
    PEP_LOG(LogTag, Severity::Info) << "Added user identifier " << Logging::Escape(x.newUid_) << " for user " << Logging::Escape(x.existingUid_);
  }
  for (auto& x : request.removeUserIdentifier_) {
    storage_->removeIdentifierForUser(x.uid_);
    PEP_LOG(LogTag, Severity::Info) << "Removed user identifier " << Logging::Escape(x.uid_);
  }
  for (auto& x : request.setPrimaryId_) {
    storage_->setPrimaryIdentifierForUser(x);
    PEP_LOG(LogTag, Severity::Info) << "Set identifier " << Logging::Escape(x) << " as primary identifier.";
  }
  for (auto& x : request.unsetPrimaryId_) {
    storage_->unsetPrimaryIdentifierForUser(x);
    PEP_LOG(LogTag, Severity::Info) << "Unset identifier " << Logging::Escape(x) << " as primary identifier.";
  }
  for (auto& x : request.setDisplayId_) {
    storage_->setDisplayIdentifierForUser(x);
    PEP_LOG(LogTag, Severity::Info) << "Set identifier " << Logging::Escape(x) << " as display identifier.";
  }
  for (auto& x : request.createUserGroup_) {
    storage_->createUserGroup(x.userGroup_);
    PEP_LOG(LogTag, Severity::Info) << "Created user group " << Logging::Escape(x.userGroup_.name_);
  }
  for (auto& x : request.removeUserGroup_) {
    storage_->removeUserGroup(x.name_);
    PEP_LOG(LogTag, Severity::Info) << "Removed user group " << Logging::Escape(x.name_);
  }
  for (auto& x : request.modifyUserGroup_) {
    storage_->modifyUserGroup(x.userGroup_);
    PEP_LOG(LogTag, Severity::Info) << "Modified user group " << Logging::Escape(x.userGroup_.name_);
  }
  for (auto& x : request.addUserToGroup_) {
    storage_->addUserToGroup(x.uid_, x.group_);
    PEP_LOG(LogTag, Severity::Info) << "Added user to user group " << Logging::Escape(x.group_);
  }
  return rxcpp::rxs::iterate(request.removeUserFromGroup_).concat_map([storage=storage_, accessManager=this->accessManager_](const RemoveUserFromGroup& x)-> rxcpp::observable<FakeVoid> {
    int64_t internalUserId = storage->getInternalUserId(x.uid_);
    storage->removeUserFromGroup(internalUserId, x.group_);
    PEP_LOG(LogTag, Severity::Info) << "Removed user from user group " << Logging::Escape(x.group_);
    if (x.blockTokens_) {
      return rxcpp::rxs::iterate(storage->getAllIdentifiersForUser(internalUserId)).concat_map([group=x.group_, accessManager](const std::string& uid) {
        TokenBlockingCreateRequest tokenBlockRequest{
          .target = {
            .subject = uid,
            .userGroup = group,
            .issueDateTime = TimeNow(),
          },
          .note = "User removed from user group",
        };
        return accessManager->keyServerProxy_.requestTokenBlockingCreate(std::move(tokenBlockRequest));
      }).op(RxInstead(FakeVoid()));
    }
    return rxcpp::rxs::just(FakeVoid());
  }).op(RxInstead(UserMutationResponse()));
}

MigrateUserDbToAccessManagerResponse AccessManager::Backend::migrateUserDb(const std::filesystem::path& dbPath) {
  return storage_->migrateUserDb(dbPath);
}

void AccessManager::Backend::ensureNoUserData() const {
  return storage_->ensureNoUserData();
}

FindUserResponse AccessManager::Backend::handleFindUserRequest(
    const FindUserRequest& request,
    const std::string& userGroup) {
  constexpr auto CaseInsensitive = Storage::CaseSensitivity::CaseInsensitive;
  constexpr auto CaseSensitive = Storage::CaseSensitivity::CaseSensitive;

  UserGroup::EnsureAccess(UserGroup::Authserver, userGroup);
  std::optional<int64_t> userId = storage_->findInternalUserId(request.primaryId_, CaseSensitive);
  if (userId) {
    auto primary = storage_->getPrimaryIdentifierForUser(*userId);
    if (!primary) {
      storage_->setPrimaryIdentifierForUser(*userId, request.primaryId_);
    }
    else if (primary != request.primaryId_) {
      PEP_LOG(LogTag, Severity::Error) << "Found a user based on the primary ID we received from the authentication source (" << request.primaryId_
        << "), but according to our storage a different id for this user is the primary ID. (" << *primary << ")";
      throw Error("There is a problem with your user account. Please contact support to resolve this issue.");
    }
  }
  else {
    userId = storage_->findInternalUserId(request.alternativeIds_, CaseInsensitive);
    if (userId) {
      auto primary = storage_->getPrimaryIdentifierForUser(*userId);
      if (!primary) {
        storage_->addIdentifierForUser(*userId, request.primaryId_, UserIdFlags::IsPrimaryId, CaseSensitive);
      }
      else{
        PEP_LOG(LogTag, Severity::Error) << "A user tried to login as a user for which we already have a primary ID (" << *primary
          << "), that does not match the primary ID we received from the authentication source (" << request.primaryId_ << ").";
        throw Error("A different user account already exists for the provided user ID. Please contact support to resolve this issue.");
      }
    }
  }
  if (!userId) {
    return FindUserResponse(std::nullopt);
  }
  return FindUserResponse(storage_->getUserGroupsForUser(*userId));
}

void AccessManager::Backend::removeParticipantFromGroup(const LocalPseudonym& localPseudonym, const std::string& group) {
  storage_->removeParticipantFromGroup(localPseudonym, group);
}
void AccessManager::Backend::addParticipantToGroup(const LocalPseudonym& localPseudonym, const std::string& group) {
  storage_->addParticipantToGroup(localPseudonym, group);
}

void AccessManager::Backend::checkParticipantAccess(const std::string& userGroup,
                                                   const LocalPseudonym& localPseudonym,
                                                   const std::vector<std::string>& modes,
                                                   Timestamp at) {
  // What ParticipantGroups is this localPseudonym in?
  auto pgps = storage_->getParticipantGroupParticipants(at, {.localPseudonyms = std::vector<LocalPseudonym>{localPseudonym}});
  std::vector<std::string> participantGroups{"*"}; // All participants are implicitly added to "*"
  participantGroups.reserve(participantGroups.size() + pgps.size());
  std::transform(pgps.cbegin(), pgps.cend(), std::back_inserter(participantGroups), [](auto& entry) {
    return entry.participantGroup;
  });

  std::vector<std::string> errorMessageParts;
  for (auto& mode : modes) {
    auto pgars = storage_->getParticipantGroupAccessRules(at, {.participantGroups = participantGroups, .userGroups = std::vector<std::string>{userGroup}, .modes = std::vector<std::string>{mode}});
    if (pgars.empty()) { // Stating the opposite, if there is an access rule for ANY of the participantGroups, all is
                         // well.
      errorMessageParts.push_back("Access denied to participant for mode " + Logging::Escape(mode));
    }
  }
  if (errorMessageParts.size() > 0) {
    throw Error(boost::algorithm::join(errorMessageParts, "\n"));
  }
}

bool AccessManager::Backend::hasLocalPseudonym(const LocalPseudonym& localPseudonym) {
  return storage_->hasLocalPseudonym(localPseudonym);
}
void AccessManager::Backend::storeLocalPseudonymAndPP(const LocalPseudonym& localPseudonym,
                                                    const PolymorphicPseudonym& polymorphicPseudonym) {
  storage_->storeLocalPseudonymAndPP(localPseudonym, polymorphicPseudonym);
}

void AccessManager::Backend::checkTicketRequest(const TicketRequest2& request) {
  if (request.accessSubjects_.size() > 0 && request.participantGroups_.size() > 0) {
    // We decided to not support this situation any more, since we don't expect this to be used often.
    // At the time of writing this comment, the code was not written with this assumption in mind, so could possibly be
    // simplified The problem we want to solve with this assumption is that if a participant group is given, as well as
    // a specific PP that is in that participant group, that participant is returned twice. This means it is printed
    // twice in e.g. pepcli list
    throw Error("The ticket request contains participant group(s) as well as specific participant(s). This is not "
                "supported. Use either groups or specific participants.");
  }

  auto duplicate = TryFindDuplicateValue(request.accessSubjects_);
  if (duplicate != std::nullopt) {
    PEP_LOG(LogTag, Severity::Error) << "Failing ticket request due to duplicate PP " << duplicate->text();
    throw Error("Ticket request failed due to duplicate polymorphic pseudonym. Please request access to unique "
                "polymorphic pseudonyms");
  }

  // Check all participantgroups and columngroups for existence
  std::vector<std::string> errorMessageParts;
  for (const auto& pg : request.participantGroups_) {
    if (!storage_->hasParticipantGroup(pg)) {
      errorMessageParts.push_back("Unknown participantgroup specified: " + Logging::Escape(pg));
    }
  }
  for (const auto& cg : request.columnGroups_) {
    if (!storage_->hasColumnGroup(cg)) {
      errorMessageParts.push_back("Unknown columngroup specified: " + Logging::Escape(cg));
    }
  }
  for (const auto& col : request.columns_) {
    if (!storage_->hasColumn(col)) {
      errorMessageParts.push_back("Unknown column specified: " + Logging::Escape(col));
    }
  }
  if (errorMessageParts.size() > 0) {
    throw Error(boost::algorithm::join(errorMessageParts, "\n"));
  }
}

void AccessManager::Backend::checkParticipantGroupAccess(std::span<const std::string> participantGroups,
                                                       const std::string& userGroup,
                                                       std::vector<std::string>& modes,
                                                       const Timestamp& timestamp) {
  using namespace std::ranges;
  if (!participantGroups.empty() && find(modes, "enumerate") == modes.cend()) {
    modes.push_back("enumerate");
  }

  if (userGroup == UserGroup::DataAdministrator && !participantGroups.empty()) {
    PEP_LOG(LogTag, Severity::Info)
        << "Granting " << Logging::Escape(userGroup)
        << " unchecked access to participant group(s): "
        << boost::algorithm::join(std::pair{participantGroups.begin(), participantGroups.end()}, ", ");
  }
  else {
    std::vector<std::string> errorMessageParts;
    ParticipantGroupAccessRuleFilter filter{
      .participantGroups = RangeToVector(participantGroups),
      .userGroups = {{userGroup}},
      .modes = {{}},
    };
    for (auto& mode : modes) {
      filter.modes->assign({mode});
      auto allowedParticipantGroups = RangeToCollection<std::unordered_set>(
        storage_->getParticipantGroupAccessRules(timestamp, filter)
        | views::transform(std::mem_fn(&ParticipantGroupAccessRule::participantGroup)));
      for (auto& pg : participantGroups) {
        if (!allowedParticipantGroups.contains(pg)) {
          errorMessageParts.push_back("Access denied to " + Logging::Escape(userGroup) + " for mode "
                                      + Logging::Escape(mode) + " to participant-group " + Logging::Escape(pg));
        }
      }
    }
    if (errorMessageParts.size() > 0) {
      throw Error(boost::algorithm::join(errorMessageParts, "\n"));
    }
  }
}

std::unordered_map<std::string, pep::IndexList> AccessManager::Backend::fillParticipantGroupMap(
    std::span<const std::string> participantGroups,
    std::vector<Pp>& pps) {
  // ParticipantGroups by Polymorph Pseudonym
  auto groupedPps = RangeToCollection<std::vector<std::pair<PolymorphicPseudonym, std::unordered_set<std::string> /*participant groups*/>>>(
    storage_->getPpGroups(participantGroups));
  std::ranges::shuffle(groupedPps, CryptoUrbg());

  std::unordered_map<std::string, pep::IndexList> participantGroupMap;
  for (const auto& [pp, groups] : groupedPps) {
    pps.emplace_back(pp, false);
    for (const std::string& pg : groups) {
      participantGroupMap[pg].indices_.push_back(static_cast<uint32_t>(pps.size() - 1));
    }
  }
  if (pps.size() > std::numeric_limits<std::uint32_t>::max())
    throw Error("Too many polymorphic pseudonyms to fill index vector");
  // Add groups without participants
  EnsureMapContains(participantGroupMap, participantGroups);
  return participantGroupMap;
}

std::unordered_map<std::string, IndexList> AccessManager::Backend::unfoldColumnGroupsAndCheckAccess(const std::string& userGroup,
                                                             const std::vector<std::string>& columnGroups,
                                                             const std::vector<std::string>& modes,
                                                             Timestamp at,
                                                             std::vector<std::string>& columns) {
  ColumnAccessRequest request;
  request.includeImplicitlyGranted = true;
  // All columns and Columngroups this usergroup has access to.
  auto columnAccess = handleColumnAccessRequest(request, userGroup);
  std::vector<std::string> errorMessageParts;

  // process columnGroups
  for (auto& cg : columnGroups) {
    auto iterator = columnAccess.columnGroups.find(cg);
    if (iterator == columnAccess.columnGroups.cend()) {
      errorMessageParts.push_back("All Access denied to " + Logging::Escape(userGroup) + " to column-group "
                                  + Logging::Escape(cg));
    }
    else {
      auto availableModes = iterator->second.modes;
      for (auto& mode : modes) {
        if (std::find(availableModes.cbegin(), availableModes.cend(), mode) == availableModes.cend()) {
          errorMessageParts.push_back("Access denied to " + Logging::Escape(userGroup) + " for mode "
                                      + Logging::Escape(mode) + " to column-group " + Logging::Escape(cg));
        }
      }
    }
  }

  // Process the loose columns
  for (auto& column : columns) {
    // What columnGroups is this column in?
    auto cgcs = storage_->getColumnGroupColumns(at, {.columns = std::vector<std::string>{column}});
    std::vector<std::string> associatedColumnGroups{};
    associatedColumnGroups.reserve(cgcs.size());
    std::transform(cgcs.cbegin(), cgcs.cend(), std::back_inserter(associatedColumnGroups), [](auto& entry) {
      return entry.columnGroup;
    });
    for (auto& requiredMode : modes) {
      bool accessGranted = false;
      for (auto& cg : associatedColumnGroups) {
        // If we find the required access mode in ANY of the associated columngroups, all is well.
        auto iterator = columnAccess.columnGroups.find(cg);
        if (iterator != columnAccess.columnGroups.cend()) {
          auto& availableModes = iterator->second.modes;
          if (std::find(availableModes.cbegin(), availableModes.cend(), requiredMode) != availableModes.cend()) {
            accessGranted = true;
            break;
          }
        }
      }
      if (accessGranted == false) {
        errorMessageParts.push_back("Access denied to " + Logging::Escape(userGroup) + " for mode "
                                    + Logging::Escape(requiredMode) + " to column " + Logging::Escape(column));
      }
    }
  }
  if (errorMessageParts.size() > 0) {
    throw Error(boost::algorithm::join(errorMessageParts, "\n"));
  }

  // We have access to all columnGroups and columns. Now finish the columnGroupMap and columns vector
  // Prepare columnGroupMap
  std::unordered_map<std::string, IndexList> columnGroupMap;
  columnGroupMap.reserve(columnGroups.size());
  std::set<ColumnGroupColumn> cgcs = {};
  if (!columnGroups.empty()) {
    cgcs = storage_->getColumnGroupColumns(at, {.columnGroups = {columnGroups}});
    for (auto& cgc : cgcs) {
      // Add the column to the columns vector if it is not already there.
      auto pos = std::find(columns.cbegin(), columns.cend(), cgc.column);
      uint32_t index = static_cast<uint32_t>(pos - columns.cbegin());
      if (pos == columns.end()) {
        columns.push_back(cgc.column);
      }

      // Add the columnGroup and column to the map
      auto& entry = columnGroupMap[cgc.columnGroup];
      if (std::find(entry.indices_.cbegin(), entry.indices_.cend(), index) == entry.indices_.cend()) {
        entry.indices_.push_back(index);
      }
    }
  }
  return columnGroupMap;
}

void AccessManager::Backend::checkTicketForEncryptionKeyRequest(std::shared_ptr<EncryptionKeyRequest> request,
                                                              const Ticket2& ticket) {
  std::unordered_set<std::string> ticketCols{ticket.columns_.begin(), ticket.columns_.end()};

  for (auto const& entry : request->entries_) {
    std::string mode;
    if (entry.keyBlindMode_ == KeyBlindMode::Blind)
      mode = "write";
    else if (entry.keyBlindMode_ == KeyBlindMode::Unblind)
      mode = "read";
    else
      throw Error("Unexpected KeyBlindMode");

    if (!ticket.hasMode(mode)) {
      std::ostringstream msg;
      msg << "Access denied: ticket does not grant access mode " << mode;
      throw Error(msg.str());
    }

    auto col = entry.metadata_.getTag();
    if (ticketCols.count(col) == 0) {
      std::ostringstream msg;
      msg << "Access denied: ticket does not grant access to column " << Logging::Escape(col);
      throw Error(msg.str());
    }
  }
}

AmaQueryResponse AccessManager::Backend::performAMAQuery(const AmaQuery& query, const std::string& userGroup) {
  const auto transform = [](const auto& inRange, auto& outRange, const auto& unary) {
    std::transform(inRange.cbegin(), inRange.cend(), std::inserter(outRange, outRange.end()), unary);
  };


  UserGroup::EnsureAccess({UserGroup::AccessAdministrator, UserGroup::DataAdministrator}, userGroup, "AmaQuery");
  AmaQueryResponse result;

  ColumnGroupColumnFilter cgcFilter;
  ColumnGroupFilter cgFilter;

  if (!query.columnFilter_.empty()){
    cgcFilter.columns = std::vector<std::string>{query.columnFilter_};
  }
  if (!query.columnGroupFilter_.empty()){
    cgcFilter.columnGroups = std::vector<std::string>{query.columnGroupFilter_};
    cgFilter.columnGroups = std::vector<std::string>{query.columnGroupFilter_};
  }

  auto timestamp = query.at_ ? *query.at_ : TimeNow(); // Not using optional<>.value_or to prevent TimeNow() from being evaluated
  // All columns in the system have a explicit relation to columnGroup '*', so they will be included here.
  auto foundColumnGroupColumns = storage_->getColumnGroupColumns(timestamp, cgcFilter);

  // Keep track of which columns are in which columnGroup. This map will contain all info necessary for further steps.
  std::map<std::string, std::vector<std::string>> columnsByColumnGroup;
  if(query.columnFilter_.empty()) {
    // If we do not filter on columns, we want to find columnGroups that have no columns assigned to them.
    // These would not show up in foundColumnGroupColumns, so add them explicitly.
    auto columngroups = storage_->getColumnGroups(timestamp, cgFilter);
    for (auto& cg : columngroups){
      columnsByColumnGroup[cg.name] = {};
    }
  }
  for (auto& cgc : foundColumnGroupColumns) {
    columnsByColumnGroup[cgc.columnGroup].push_back(cgc.column);
  }

  // Find the cgars
  ColumnGroupAccessRuleFilter cgarFilter {};
  if(!query.userGroupFilter_.empty()){
    cgarFilter.userGroups = std::vector<std::string>{query.userGroupFilter_};
  }
  if(!query.columnGroupModeFilter_.empty()){
    cgarFilter.modes = std::vector<std::string>{query.columnGroupModeFilter_};
  }
  if(!query.columnFilter_.empty() || !query.columnGroupFilter_.empty()){
    cgarFilter.columnGroups = RangeToVector(std::views::keys(columnsByColumnGroup));
  }
  auto cgars = storage_->getColumnGroupAccessRules(timestamp, cgarFilter);

  if(!query.userGroupFilter_.empty() || !query.columnGroupModeFilter_.empty()) {
    // If there were additional cgar filters in place, we need to go back on the found columngroups and columns and apply another narrowing filter, showing only those
    // columngroups that appear in the cgars.
    std::set<std::string> cgsInCgars{};
    transform(cgars, cgsInCgars, [] (const auto& cgar){return cgar.columnGroup;});
    std::erase_if(columnsByColumnGroup,
                  [&cgsInCgars](const auto& entry){ return !cgsInCgars.contains(entry.first);});
  }
  // Fill the result with the columns, columnGroups and cgars
  result.columnGroups_.reserve(columnsByColumnGroup.size());
  std::set<std::string> columns{};
  for (auto& [cg, cols] : columnsByColumnGroup){
    result.columnGroups_.push_back(AmaQRColumnGroup(cg, cols));
    std::copy(cols.begin(), cols.end(), std::inserter(columns, columns.end())); // Add the found values to the columns vector.
  }
  transform(columns, result.columns_, [](const auto& col) { return AmaQRColumn(col);});

  result.columnGroupAccessRules_.reserve(cgars.size());
  transform(cgars, result.columnGroupAccessRules_, [](const auto& cgar){ return AmaQRColumnGroupAccessRule(cgar.columnGroup, cgar.userGroup, cgar.mode);});

  // Participantgroups and pgars
  ParticipantGroupFilter pgFilter;
  ParticipantGroupAccessRuleFilter pgarFilter;

  if(!query.participantGroupFilter_.empty()){
    pgFilter.participantGroups = std::vector<std::string>{query.participantGroupFilter_};
    pgarFilter.participantGroups = std::vector<std::string>{query.participantGroupFilter_};
  }
  if(!query.participantGroupModeFilter_.empty()){
    pgarFilter.modes = std::vector<std::string>{query.participantGroupModeFilter_};
  }
  if(!query.userGroupFilter_.empty()){
    pgarFilter.userGroups = std::vector<std::string>{query.userGroupFilter_};
  }

  std::set<std::string> foundParticipantGroups{};
  auto pgars = storage_->getParticipantGroupAccessRules(timestamp, pgarFilter);

  if(!query.participantGroupModeFilter_.empty() || !query.userGroupFilter_.empty()){
    // The pgar filters are narrowing the found participants as well, only show pgs with pgars
    transform(pgars, foundParticipantGroups, [](const auto& pgar) { return pgar.participantGroup;});
  } else{
    // Get the participantgroups as normal.
    auto pgs = storage_->getParticipantGroups(timestamp, pgFilter);
    transform(pgs, foundParticipantGroups,[](const auto& pg) { return pg.name;});
  }

  // Fill the result
  result.participantGroupAccessRules_.reserve(pgars.size());
  transform(pgars, result.participantGroupAccessRules_, [](const auto& pgar){return AmaQRParticipantGroupAccessRule(pgar.participantGroup, pgar.userGroup, pgar.mode);});
  result.participantGroups_.assign(foundParticipantGroups.begin(), foundParticipantGroups.end()); // Makes use of implicit string to AmaQRParticipantGroup conversion.

  return result;
}

UserQueryResponse AccessManager::Backend::performUserQuery(const UserQuery& query, const std::string& userGroup) {
  UserGroup::EnsureAccess({UserGroup::AccessAdministrator}, userGroup, "Querying users");

  return storage_->executeUserQuery(query);
}

ColumnAccess AccessManager::Backend::handleColumnAccessRequest(const ColumnAccessRequest& request,
                                                             const std::string& userGroup) {
  ColumnAccess result;
  auto now = TimeNow();

  if (request.includeImplicitlyGranted
      && userGroup == UserGroup::DataAdministrator) { // Data administrator has implicit "read-meta" access to all
                                                       // column groups
    auto allCgs = storage_->getColumnGroups(now);
    for (const auto& cg : allCgs) {
      auto& modes = result.columnGroups[cg.name].modes;
      auto end = modes.cend();
      if (std::find(modes.cbegin(), end, "read-meta") == end) {
        modes.push_back("read-meta");
      }
    }
  }

  auto cgars = storage_->getColumnGroupAccessRules(now, {.userGroups = std::vector<std::string>{userGroup}});
  for (auto& cgar : cgars) {
    auto& allowedModes = result.columnGroups[cgar.columnGroup].modes;
    allowedModes.push_back(cgar.mode);
    if (request.includeImplicitlyGranted) {
      // All users have implicit "read-meta" access if they have "read" access
      if (cgar.mode == "read" && std::find(allowedModes.cbegin(), allowedModes.cend(), "read-meta") == allowedModes.cend()) {
        allowedModes.push_back("read-meta");
      }
      // All users have implicit "write" access if they have "write-meta" access
      else if (cgar.mode == "write-meta" && std::find(allowedModes.cbegin(), allowedModes.cend(), "write") == allowedModes.cend()) {
        allowedModes.push_back("write");
      }
    }
  }

  // Remove column groups from the result that don't provide all required modes
  for (const auto& requireMode : request.requireModes) {
    // Removing items from unordered_map during iteration: see https://stackoverflow.com/a/15662547
    auto i = result.columnGroups.begin();
    while (i != result.columnGroups.end()) {
      const auto& availableModes = i->second.modes;
      if (std::find(availableModes.cbegin(), availableModes.cend(), requireMode) == availableModes.cend()) {
        i = result.columnGroups.erase(i);
      }
      else {
        ++i;
      }
    }
  }

  std::vector<std::string> columnGroupsInMap;
  columnGroupsInMap.reserve(result.columnGroups.size());
  std::transform(result.columnGroups.begin(),
                 result.columnGroups.end(),
                 std::back_inserter(columnGroupsInMap),
                 [](auto& entry) { return entry.first; });
  // For each columnGroup in the result, look up all associated columns and add them to both the "columns" vector, and
  // the groupProperties in the map.
  for (auto& cgc : storage_->getColumnGroupColumns(now, {.columnGroups = columnGroupsInMap})) {
    auto begin = result.columns.begin(), end = result.columns.end();
    auto pos = std::find(begin, end, cgc.column);
    uint32_t index = static_cast<uint32_t>(pos - begin);
    if (pos == end) {
      result.columns.push_back(cgc.column);
    }
    result.columnGroups.at(cgc.columnGroup).columns.indices_.push_back(index);
  }

  return result;
}

ParticipantGroupAccess AccessManager::Backend::handleParticipantGroupAccessRequest(
    const ParticipantGroupAccessRequest& request, const std::string& userGroup) {
  ParticipantGroupAccess result;
  auto now = TimeNow();
  if (request.includeImplicitlyGranted
      && userGroup == UserGroup::DataAdministrator) { // Data administrator has implicit full access to all participant
                                                       // groups
    auto participantGroups = storage_->getParticipantGroups(now);
    // Include participant group "*", which is not defined explicitly in the table
    // See https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2225#note_30014
    [[maybe_unused]] auto emplaced = participantGroups.insert(ParticipantGroup("*"));
    assert(emplaced.second);

    for (auto& pg : participantGroups) {
      result.participantGroups.emplace(pg.name, std::vector<std::string>({"access", "enumerate"}));
    }
  }
  else {
    // Not a Data Admin: retrieve all participant groups to which the access group has access
    auto pgars = storage_->getParticipantGroupAccessRules(now, {.userGroups = std::vector<std::string>{userGroup}});
    for (auto& pgar : pgars) {
      auto& modes = result.participantGroups[pgar.participantGroup];
      modes.push_back(pgar.mode);
    }
  }
  return result;
}

ColumnNameMappingResponse AccessManager::Backend::handleColumnNameMappingRequest(const ColumnNameMappingRequest& request,
                                                                               const std::string& userGroup) {
  ColumnNameMappingResponse response;

  // Mappings can be read by any user
  if (request.action == CrudAction::Read) {
    if (request.original.has_value()) {
      auto mapping = storage_->getColumnNameMapping(*request.original);
      if (mapping.has_value()) {
        response.mappings.emplace_back(*mapping);
      }
    }
    else {
      response.mappings = storage_->getAllColumnNameMappings();
    }
    return response;
  }

  // Mappings can be managed only by Data Admin
  UserGroup::EnsureAccess({UserGroup::DataAdministrator}, userGroup);

  if (!request.original.has_value()) {
    throw Error("Original name not specified");
  }
  const auto& original = *request.original;

  switch (request.action) {
  case CrudAction::Create:
    if (!request.mapped.has_value()) {
      throw Error("Mapped name not specified");
    }
    response.mappings.emplace_back(ColumnNameMapping{original, *request.mapped});
    assert(response.mappings.size() == 1U);
    storage_->createColumnNameMapping(response.mappings.front());
    break;
  case CrudAction::Update:
    if (!request.mapped.has_value()) {
      throw Error("Mapped name not specified");
    }
    response.mappings.emplace_back(ColumnNameMapping{original, *request.mapped});
    assert(response.mappings.size() == 1U);
    storage_->updateColumnNameMapping(response.mappings.front());
    break;
  case CrudAction::Delete:
    storage_->deleteColumnNameMapping(original);
    assert(response.mappings.empty());
    break;
  default:
    throw Error("Unsupported action " + std::to_string(request.action));
  }

  assert(response.mappings.size() <= 1U);
  return response;
}

std::vector<StructureMetadataEntry> AccessManager::Backend::handleStructureMetadataRequest(
    const StructureMetadataRequest& request,
    const std::string& userGroup) {
  EnsureStructureMetadataAccess(AccessMode::Read, request.subjectType, userGroup);

  const Timestamp now = TimeNow();
  return {storage_->getStructureMetadata(
      now,
      request.subjectType,
      {
          .subjects = request.subjects,
          .keys = request.keys,
      })};
}

void AccessManager::Backend::handleSetStructureMetadataRequestHead(
    const SetStructureMetadataRequest& request,
    const std::string& userGroup) {
  EnsureStructureMetadataAccess(AccessMode::Write, request.subjectType, userGroup);

  for (const auto& [subject, key] : request.remove) {
    storage_->removeStructureMetadata(request.subjectType, subject, key);
  }
}

void AccessManager::Backend::handleSetStructureMetadataRequestEntry(
    StructureMetadataType subjectType,
    const StructureMetadataEntry& entry,
    const std::string& userGroup) {
  EnsureStructureMetadataAccess(AccessMode::Write, subjectType, userGroup);

  storage_->setStructureMetadata(subjectType, entry.subjectKey.subject, entry.subjectKey.key, entry.value);
}

std::filesystem::path AccessManager::Backend::getStoragePath() {
  return storage_->getPath();
}

std::vector<std::string> AccessManager::Backend::getChecksumChainNames() {
  return storage_->getChecksumChainNames();
}
void AccessManager::Backend::computeChecksum(const std::string& chain,
                                           std::optional<uint64_t> maxCheckpoint,
                                           uint64_t& checksum,
                                           uint64_t& checkpoint) {
  return storage_->computeChecksum(chain, maxCheckpoint, checksum, checkpoint);
}

} // namespace pep
