#include <pep/accessmanager/Storage.hpp>
#include <pep/auth/UserGroups.hpp>
#include <pep/crypto/CPRNG.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/MiscUtil.hpp>

#include <boost/algorithm/string/join.hpp>

namespace pep {

const std::string LOG_TAG("AccessManager::Backend");

template <typename TValue>
void EnsureMapContains(std::unordered_map<std::string, TValue>& map, const std::vector<std::string>& keys) {
  std::for_each(keys.cbegin(), keys.cend(), [&map](const std::string& key) {
    [[maybe_unused]] auto position = map.try_emplace(key).first;
    assert(position != map.cend());
    assert(position == map.find(key));
  });
}

AccessManager::Backend::Backend(const std::filesystem::path& path, std::shared_ptr<GlobalConfiguration> globalConf)
  : Backend(std::make_shared<AccessManager::Backend::Storage>(path, globalConf)) {
}

/********** START AMA Operations For Requests **********/
void AccessManager::Backend::createColumnsForRequest(const AmaMutationRequest& amRequest) {
  for (auto& mutation : amRequest.mCreateColumn) {
    this->mStorage->createColumn(mutation.mName);
    LOG(LOG_TAG, info) << "Created column " << Logging::Escape(mutation.mName);
  }
}
void AccessManager::Backend::removeColumnsForRequest(const AmaMutationRequest& amRequest) {
  for (auto& mutation : amRequest.mRemoveColumn) {
    this->mStorage->removeColumn(mutation.mName);
    LOG(LOG_TAG, info) << "Removed column " << Logging::Escape(mutation.mName);
  }
}
void AccessManager::Backend::createColumnGroupsForRequest(const AmaMutationRequest& amRequest) {
  for (auto& mutation : amRequest.mCreateColumnGroup) {
    this->mStorage->createColumnGroup(mutation.mName);
    LOG(LOG_TAG, info) << "Created columngroup " << Logging::Escape(mutation.mName);
  }
}
void AccessManager::Backend::removeColumnGroupsForRequest(const AmaMutationRequest& amRequest) {
  for (auto& mutation : amRequest.mRemoveColumnGroup) {
    this->mStorage->removeColumnGroup(mutation.mName, amRequest.mForceColumnGroupRemoval);
    LOG(LOG_TAG, info) << "Removed columngroup " << Logging::Escape(mutation.mName);
  }
}
void AccessManager::Backend::addColumnsToGroupsForRequest(const AmaMutationRequest& amRequest) {
  for (auto& mutation : amRequest.mAddColumnToGroup) {
    this->mStorage->addColumnToGroup(mutation.mColumn, mutation.mColumnGroup);
    LOG(LOG_TAG, info) << "Added column " << Logging::Escape(mutation.mColumn) << " to group "
                                               << Logging::Escape(mutation.mColumnGroup);
  }
}
void AccessManager::Backend::removeColumnsFromGroupsForRequest(const AmaMutationRequest& amRequest) {
  for (auto& mutation : amRequest.mRemoveColumnFromGroup) {
    this->mStorage->removeColumnFromGroup(mutation.mColumn, mutation.mColumnGroup);
    LOG(LOG_TAG, info) << "Removed column " << Logging::Escape(mutation.mColumn)
                                               << " from group " << Logging::Escape(mutation.mColumnGroup);
  }
}
void AccessManager::Backend::createParticipantGroupsForRequest(const AmaMutationRequest& amRequest) {
  for (auto& mutation : amRequest.mCreateParticipantGroup) {
    this->mStorage->createParticipantGroup(mutation.mName);
    LOG(LOG_TAG, info)
        << "Created participant group via ref " << Logging::Escape(mutation.mName);
  }
}
void AccessManager::Backend::removeParticipantGroupsForRequest(const AmaMutationRequest& amRequest) {
  for (auto& mutation : amRequest.mRemoveParticipantGroup) {
    this->mStorage->removeParticipantGroup(mutation.mName, amRequest.mForceParticipantGroupRemoval);
    LOG(LOG_TAG, info) << "Removed participant group " << Logging::Escape(mutation.mName);
  }
}
void AccessManager::Backend::createColumnGroupAccessRulesForRequest(const AmaMutationRequest& amRequest) {
  for (auto& mutation : amRequest.mCreateColumnGroupAccessRule) {
    this->mStorage->createColumnGroupAccessRule(mutation.mColumnGroup, mutation.mUserGroup, mutation.mMode);
    LOG(LOG_TAG, info)
        << "Created column-group-access-rule: " << Logging::Escape(mutation.mUserGroup) << " has access to mode "
        << Logging::Escape(mutation.mMode) << " for column group " << Logging::Escape(mutation.mColumnGroup);
  }
}
void AccessManager::Backend::removeColumnGroupAccessRulesForRequest(const AmaMutationRequest& amRequest) {
  for (auto& mutation : amRequest.mRemoveColumnGroupAccessRule) {
    this->mStorage->removeColumnGroupAccessRule(mutation.mColumnGroup, mutation.mUserGroup, mutation.mMode);
    LOG(LOG_TAG, info)
        << "Removed column-group-access-rule: " << Logging::Escape(mutation.mUserGroup)
        << " no longer has access to mode " << Logging::Escape(mutation.mMode) << " for column group "
        << Logging::Escape(mutation.mColumnGroup);
  }
}
void AccessManager::Backend::createParticipantGroupAccessRulesForRequest(const AmaMutationRequest& amRequest) {
  for (auto& mutation : amRequest.mCreateParticipantGroupAccessRule) {
    this->mStorage->createParticipantGroupAccessRule(mutation.mParticipantGroup, mutation.mUserGroup, mutation.mMode);
    LOG(LOG_TAG, info)
        << "Created group-access-rule: " << Logging::Escape(mutation.mUserGroup) << " has access to mode "
        << Logging::Escape(mutation.mMode) << " for group " << Logging::Escape(mutation.mParticipantGroup);
  }
}
void AccessManager::Backend::removeParticipantGroupAccessRulesForRequest(const AmaMutationRequest& amRequest) {
  for (auto& mutation : amRequest.mRemoveParticipantGroupAccessRule) {
    this->mStorage->removeParticipantGroupAccessRule(mutation.mParticipantGroup, mutation.mUserGroup, mutation.mMode);
    LOG(LOG_TAG, info)
        << "Removed group-access-rule: " << Logging::Escape(mutation.mUserGroup) << " no longer has access to mode "
        << Logging::Escape(mutation.mMode) << " for group " << Logging::Escape(mutation.mParticipantGroup);
  }
}

void AccessManager::Backend::performMutationsForRequest(const AmaMutationRequest& request, const std::string& userGroup) {

  // Check for required access groups for some operations.
  if (request.hasDataAdminOperation())
    user_group::EnsureAccess({user_group::DataAdministrator}, userGroup);
  if (request.hasAccessAdminOperation())
    user_group::EnsureAccess({user_group::AccessAdministrator}, userGroup);

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

void AccessManager::Backend::removeParticipantFromGroup(const LocalPseudonym& localPseudonym, const std::string& group) {
  mStorage->removeParticipantFromGroup(localPseudonym, group);
}
void AccessManager::Backend::addParticipantToGroup(const LocalPseudonym& localPseudonym, const std::string& group) {
  mStorage->addParticipantToGroup(localPseudonym, group);
}

void AccessManager::Backend::assertParticipantAccess(const std::string& userGroup,
                                                   const LocalPseudonym& localPseudonym,
                                                   const std::vector<std::string>& modes,
                                                   Timestamp at) {
  // What ParticipantGroups is this localPseudonym in?
  auto pgps = mStorage->getParticipantGroupParticipants(at, {.localPseudonyms = std::vector<LocalPseudonym>{localPseudonym}});
  std::vector<std::string> participantGroups{"*"}; // All participants are implicitly added to "*"
  participantGroups.reserve(participantGroups.size() + pgps.size());
  std::transform(pgps.cbegin(), pgps.cend(), std::back_inserter(participantGroups), [](auto& entry) {
    return entry.participantGroup;
  });

  std::vector<std::string> errorMessageParts;
  for (auto& mode : modes) {
    auto pgars = mStorage->getParticipantGroupAccessRules(at, {.participantGroups = participantGroups, .userGroups = std::vector<std::string>{userGroup}, .modes = std::vector<std::string>{mode}});
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
  return mStorage->hasLocalPseudonym(localPseudonym);
}
void AccessManager::Backend::storeLocalPseudonymAndPP(const LocalPseudonym& localPseudonym,
                                                    const PolymorphicPseudonym& polymorphicPseudonym) {
  mStorage->storeLocalPseudonymAndPP(localPseudonym, polymorphicPseudonym);
}

void AccessManager::Backend::checkTicketRequest(const TicketRequest2& request) {
  if (request.mPolymorphicPseudonyms.size() > 0 && request.mParticipantGroups.size() > 0) {
    // We decided to not support this situation any more, since we don't expect this to be used often.
    // At the time of writing this comment, the code was not written with this assumption in mind, so could possibly be
    // simplified The problem we want to solve with this assumption is that if a participant group is given, as well as
    // a specific PP that is in that participant group, that participant is returned twice. This means it is printed
    // twice in e.g. pepcli list
    throw Error("The ticket request contains participant group(s) as well as specific participant(s). This is not "
                "supported. Use either groups or specific participants.");
  }

  auto duplicate = TryFindDuplicateValue(request.mPolymorphicPseudonyms);
  if (duplicate != std::nullopt) {
    LOG(LOG_TAG, error) << "Failing ticket request due to duplicate PP " << duplicate->text();
    throw Error("Ticket request failed due to duplicate polymorphic pseudonym. Please request access to unique "
                "polymorphic pseudonyms");
  }

  // Check all participantgroups and columngroups for existence
  std::vector<std::string> errorMessageParts;
  for (const auto& pg : request.mParticipantGroups) {
    if (!mStorage->hasParticipantGroup(pg)) {
      errorMessageParts.push_back("Unknown participantgroup specified: " + Logging::Escape(pg));
    }
  }
  for (const auto& cg : request.mColumnGroups) {
    if (!mStorage->hasColumnGroup(cg)) {
      errorMessageParts.push_back("Unknown columngroup specified: " + Logging::Escape(cg));
    }
  }
  for (const auto& col : request.mColumns) {
    if (!mStorage->hasColumn(col)) {
      errorMessageParts.push_back("Unknown column specified: " + Logging::Escape(col));
    }
  }
  if (errorMessageParts.size() > 0) {
    throw Error(boost::algorithm::join(errorMessageParts, "\n"));
  }
}

void AccessManager::Backend::checkParticipantGroupAccess(const std::vector<std::string>& participantGroups,
                                                       const std::string& userGroup,
                                                       std::vector<std::string>& modes,
                                                       const Timestamp& timestamp) {
  if (!participantGroups.empty() && std::find(modes.cbegin(), modes.cend(), "enumerate") == modes.cend()) {
    modes.push_back("enumerate");
  }

  if (userGroup == user_group::DataAdministrator && !participantGroups.empty()) {
    LOG(LOG_TAG, info)
        << "Granting " << Logging::Escape(userGroup)
        << " unchecked access to participant group(s): " << boost::algorithm::join(participantGroups, ", ");
  }
  else {
    std::vector<std::string> errorMessageParts;
    for (auto& mode : modes) {
      auto pgars = mStorage->getParticipantGroupAccessRules(timestamp, {participantGroups, std::vector<std::string>{userGroup}, std::vector<std::string>{mode}});
      std::vector<std::string> allowedParticipantGroups;
      allowedParticipantGroups.reserve(pgars.size());
      std::transform(pgars.cbegin(), pgars.cend(), std::back_inserter(allowedParticipantGroups), [](auto& entry) {
        return entry.participantGroup;
      });
      for (auto& pg : participantGroups) {
        if (find(allowedParticipantGroups.cbegin(), allowedParticipantGroups.cend(), pg)
            == allowedParticipantGroups.end()) {
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

void AccessManager::Backend::fillParticipantGroupMap(const std::vector<std::string>& participantGroups,
                                                   std::vector<pp_t>& prePPs,
                                                   std::unordered_map<std::string, IndexList>& participantGroupMap) {
  // ParticipantGroups by Polymorph Pseudonym
  auto groupedPps = mStorage->getPPs(participantGroups);
  auto urbg = CPURBG();
  while (!groupedPps.empty()) {
    auto randomIt = std::next(groupedPps.begin(), static_cast<ptrdiff_t>(urbg() % groupedPps.size()));
    prePPs.push_back(pp_t(randomIt->first, false));
    for (auto& pg : randomIt->second) {
      participantGroupMap[pg].mIndices.push_back(static_cast<uint32_t>(prePPs.size() - 1));
    }
    groupedPps.erase(randomIt);
  }
  if (prePPs.size() > UINT_MAX)
    throw Error("Too many polymorphic pseudonyms to fill index vector");
  EnsureMapContains(participantGroupMap, participantGroups);
}

void AccessManager::Backend::unfoldColumnGroupsAndAssertAccess(const std::string& userGroup,
                                                             const std::vector<std::string>& columnGroups,
                                                             const std::vector<std::string>& modes,
                                                             Timestamp at,
                                                             std::vector<std::string>& columns,
                                                             std::unordered_map<std::string, IndexList>&
                                                                 columnGroupMap) {
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
    auto cgcs = mStorage->getColumnGroupColumns(at, {.columns = std::vector<std::string>{column}});
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
  columnGroupMap.clear();
  columnGroupMap.reserve(columnGroups.size());
  std::set<ColumnGroupColumn> cgcs = {};
  if (!columnGroups.empty()) {
    cgcs = mStorage->getColumnGroupColumns(at, {.columnGroups = {columnGroups}});
    for (auto& cgc : cgcs) {
      // Add the column to the columns vector if it is not already there.
      auto pos = std::find(columns.cbegin(), columns.cend(), cgc.column);
      uint32_t index = static_cast<uint32_t>(pos - columns.cbegin());
      if (pos == columns.end()) {
        columns.push_back(cgc.column);
      }

      // Add the columnGroup and column to the map
      auto& entry = columnGroupMap[cgc.columnGroup];
      if (std::find(entry.mIndices.cbegin(), entry.mIndices.cend(), index) == entry.mIndices.cend()) {
        entry.mIndices.push_back(index);
      }
    }
  }
}

void AccessManager::Backend::checkTicketForEncryptionKeyRequest(std::shared_ptr<EncryptionKeyRequest> request,
                                                              const Ticket2& ticket) {
  std::unordered_set<std::string> ticketCols{ticket.mColumns.begin(), ticket.mColumns.end()};

  for (auto const& entry : request->mEntries) {
    std::string mode;
    if (entry.mKeyBlindMode == KeyBlindMode::BLIND_MODE_BLIND)
      mode = "write";
    else if (entry.mKeyBlindMode == KeyBlindMode::BLIND_MODE_UNBLIND)
      mode = "read";
    else
      throw Error("Unexpected KeyBlindMode");

    if (!ticket.hasMode(mode)) {
      std::ostringstream msg;
      msg << "Access denied: ticket does not grant access mode " << mode;
      throw Error(msg.str());
    }

    auto col = entry.mMetadata.getTag();
    if (ticketCols.count(col) == 0) {
      std::ostringstream msg;
      msg << "Access denied: ticket does not grant access to column " << Logging::Escape(col);
      throw Error(msg.str());
    }
  }
}

pep::AmaQueryResponse AccessManager::Backend::performAMAQuery(const AmaQuery& request, const std::string& userGroup) {
  user_group::EnsureAccess({user_group::AccessAdministrator, user_group::DataAdministrator}, userGroup, "AmaQuery");

  AmaQueryResponse result;

  // If there are any filters related to a part of the query for either groups, we want the other sections to be
  // filtered accordingly. e.g. If there is a columnFilter, we only want to return columnGroups containing that column.
  bool filterColumnRelated = !request.mColumnFilter.empty() || !request.mColumnGroupFilter.empty()
                          || !request.mColumnGroupModeFilter.empty() || !request.mUserGroupFilter.empty();
  bool filterParticipantRelated = !request.mParticipantGroupFilter.empty()
                               || !request.mParticipantGroupModeFilter.empty() || !request.mUserGroupFilter.empty();

  // Start with the most restrictive results, being the access rules. If no specific value was given, all is returned.
  std::set<std::string> targetColumnGroups{};
  ColumnGroupAccessRuleFilter cgarFilter;
  if (!request.mColumnGroupFilter.empty()) {
    cgarFilter.columnGroups= std::vector<std::string>{request.mColumnGroupFilter};
    targetColumnGroups.insert(request.mColumnGroupFilter);
  }
  if (!request.mUserGroupFilter.empty()) {
    cgarFilter.userGroups = std::vector<std::string>{request.mUserGroupFilter};
  }
  if (!request.mColumnGroupModeFilter.empty()) {
    cgarFilter.modes = std::vector<std::string>{request.mColumnGroupModeFilter};
  }
  auto cgars = mStorage->getColumnGroupAccessRules(request.mAt, cgarFilter);

  result.mColumnGroupAccessRules.reserve(cgars.size());
  std::transform(cgars.cbegin(), cgars.cend(), std::back_inserter(result.mColumnGroupAccessRules),
                 [&targetColumnGroups, filterColumnRelated](auto& cgar) {
                   if (filterColumnRelated) {
                    // use the transform loop to also add all columngroups that are mentioned in the found access rules to the targetColumnGroups
                    targetColumnGroups.insert(cgar.columnGroup);
                   }
                   return AmaQRColumnGroupAccessRule(cgar.columnGroup, cgar.userGroup, cgar.mode);
                 });

  ColumnGroupColumnFilter cgcFilter;
  ColumnGroupFilter cgFilter;
  if (filterColumnRelated) {
    if (!request.mColumnGroupFilter.empty()) {
      // Look only for the specific columnGroup that was mentioned in the request.
      cgFilter.columnGroups = std::vector<std::string>{request.mColumnGroupFilter};
      cgcFilter.columnGroups = std::vector<std::string>{request.mColumnGroupFilter};
    }
    else {
      // Look for all columnGroups found in the access rules.
      cgFilter.columnGroups = {};
      cgcFilter.columnGroups = {};
      cgFilter.columnGroups->assign(targetColumnGroups.cbegin(), targetColumnGroups.cend());
      cgcFilter.columnGroups->assign(targetColumnGroups.cbegin(), targetColumnGroups.cend());
    }
    if (!request.mColumnFilter.empty()) {
      // If a column was mentioned in the request, add it to the filter.
      cgcFilter.columns = std::vector<std::string>{request.mColumnFilter};
    }
  }
  auto cgs = mStorage->getColumnGroups(request.mAt, cgFilter);
  auto cgcs = mStorage->getColumnGroupColumns(request.mAt, cgcFilter);

  std::set<std::string> targetColumns{};
  std::map<std::string, std::vector<std::string>> columnsByColumnGroup;
  for (auto& cgc : cgcs) {
    columnsByColumnGroup[cgc.columnGroup].push_back(cgc.column); // Add all columnGroups with columns to the map
    targetColumns.insert(cgc.column);
  }
  result.mColumnGroups.reserve(cgs.size());
  std::transform(cgs.cbegin(), cgs.cend(), std::back_inserter(result.mColumnGroups), [&columnsByColumnGroup](auto cg) {
    std::vector<std::string> cols{};
    auto iterator = columnsByColumnGroup.find(cg.name);
    if (iterator != columnsByColumnGroup.end()) {
      cols = iterator->second;
    }
    return AmaQRColumnGroup(cg.name, cols);
  });

  ColumnFilter cFilter;
  if (filterColumnRelated) {
    if (!request.mColumnFilter.empty()) {
      // Look only for the specific column that was mentioned in the request.
      cFilter.columns = std::vector<std::string>{request.mColumnFilter};
    }
    else {
      // Look for all columns found in the columnGroupColumns.
      cFilter.columns={};
      cFilter.columns->assign(targetColumns.cbegin(), targetColumns.cend());
    }
  }
  auto columns = mStorage->getColumns(request.mAt, cFilter);
  for (auto& col : columns) {
    result.mColumns.push_back(AmaQRColumn(col.name));
  }

  // Do the same thing, now for participantGroups. This is slightly less complicated because we do not return single participants.
  std::set<std::string> targetParticipantGroups;
  ParticipantGroupAccessRuleFilter pgarFilter;
  if (!request.mParticipantGroupFilter.empty()) {
    pgarFilter.participantGroups = std::vector<std::string>{request.mParticipantGroupFilter};
    targetParticipantGroups.insert(request.mParticipantGroupFilter);
  }
  if (!request.mUserGroupFilter.empty()) {
    pgarFilter.userGroups = std::vector<std::string>{request.mUserGroupFilter};
  }
  if (!request.mParticipantGroupModeFilter.empty()) {
    pgarFilter.modes = std::vector<std::string>{request.mParticipantGroupModeFilter};
  }
  auto pgars = mStorage->getParticipantGroupAccessRules(request.mAt, pgarFilter);
  result.mParticipantGroupAccessRules.reserve(pgars.size());
  std::transform(pgars.cbegin(), pgars.cend(), std::back_inserter(result.mParticipantGroupAccessRules),
                 [&targetParticipantGroups, filterParticipantRelated](auto& pgar) {
                   if (filterParticipantRelated) {
                     targetParticipantGroups.insert(pgar.participantGroup);
                   }
                   return AmaQRParticipantGroupAccessRule(pgar.participantGroup, pgar.userGroup, pgar.mode);
                 });
  ParticipantGroupFilter pgFilter;
  if (filterParticipantRelated) {
    pgFilter.participantGroups = {};
    pgFilter.participantGroups->assign(targetParticipantGroups.cbegin(), targetParticipantGroups.cend());
  }
  auto pgs = mStorage->getParticipantGroups(request.mAt, pgFilter);
  result.mParticipantGroups.reserve(pgs.size());
  std::transform(pgs.cbegin(), pgs.cend(), std::back_inserter(result.mParticipantGroups),
                 [](auto& pg) { return AmaQRParticipantGroup(pg.name); });
  return result;
}

ColumnAccess AccessManager::Backend::handleColumnAccessRequest(const ColumnAccessRequest& request,
                                                             const std::string& userGroup) {
  ColumnAccess result;
  auto now = Timestamp();

  if (request.includeImplicitlyGranted
      && userGroup == user_group::DataAdministrator) { // Data administrator has implicit "read-meta" access to all
                                                       // column groups
    auto allCgs = mStorage->getColumnGroups(now);
    for (const auto& cg : allCgs) {
      auto& modes = result.columnGroups[cg.name].modes;
      auto end = modes.cend();
      if (std::find(modes.cbegin(), end, "read-meta") == end) {
        modes.push_back("read-meta");
      }
    }
  }

  auto cgars = mStorage->getColumnGroupAccessRules(now, {.userGroups = std::vector<std::string>{userGroup}});
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
  for (auto& cgc : mStorage->getColumnGroupColumns(now, {.columnGroups = columnGroupsInMap})) {
    auto begin = result.columns.begin(), end = result.columns.end();
    auto pos = std::find(begin, end, cgc.column);
    uint32_t index = static_cast<uint32_t>(pos - begin);
    if (pos == end) {
      result.columns.push_back(cgc.column);
    }
    result.columnGroups.at(cgc.columnGroup).columns.mIndices.push_back(index);
  }

  return result;
}

ParticipantGroupAccess AccessManager::Backend::handleParticipantGroupAccessRequest(
    const ParticipantGroupAccessRequest& request, const std::string& userGroup) {
  ParticipantGroupAccess result;
  auto now = Timestamp();
  if (request.includeImplicitlyGranted
      && userGroup == user_group::DataAdministrator) { // Data administrator has implicit full access to all participant
                                                       // groups
    auto participantGroups = mStorage->getParticipantGroups(now);
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
    auto pgars = mStorage->getParticipantGroupAccessRules(now, {.userGroups = std::vector<std::string>{userGroup}});
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
      auto mapping = mStorage->getColumnNameMapping(*request.original);
      if (mapping.has_value()) {
        response.mappings.emplace_back(*mapping);
      }
    }
    else {
      response.mappings = mStorage->getAllColumnNameMappings();
    }
    return response;
  }

  // Mappings can be managed only by Data Admin
  user_group::EnsureAccess({user_group::DataAdministrator}, userGroup);

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
    mStorage->createColumnNameMapping(response.mappings.front());
    break;
  case CrudAction::Update:
    if (!request.mapped.has_value()) {
      throw Error("Mapped name not specified");
    }
    response.mappings.emplace_back(ColumnNameMapping{original, *request.mapped});
    assert(response.mappings.size() == 1U);
    mStorage->updateColumnNameMapping(response.mappings.front());
    break;
  case CrudAction::Delete:
    mStorage->deleteColumnNameMapping(original);
    assert(response.mappings.empty());
    break;
  default:
    throw Error("Unsupported action " + std::to_string(request.action));
  }

  assert(response.mappings.size() <= 1U);
  return response;
}

std::filesystem::path AccessManager::Backend::getStoragePath() {
  return mStorage->getPath();
}

std::vector<std::string> AccessManager::Backend::getChecksumChainNames() {
  return mStorage->getChecksumChainNames();
}
void AccessManager::Backend::computeChecksum(const std::string& chain,
                                           std::optional<uint64_t> maxCheckpoint,
                                           uint64_t& checksum,
                                           uint64_t& checkpoint) {
  return mStorage->computeChecksum(chain, maxCheckpoint, checksum, checkpoint);
}

} // namespace pep
