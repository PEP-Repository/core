#include <gtest/gtest.h>
#include <pep/utils/File.hpp>
#include <pep/structure/StructureSerializers.hpp>
#include <pep/accessmanager/tests/TestSuiteGlobalConfiguration.hpp>

#include <pep/accessmanager/Storage.hpp>

#include <chrono>
#include <filesystem>
#include <thread>

using namespace pep;
using namespace std::literals;
using namespace std::ranges;

using namespace std::chrono_literals;
using namespace std::string_literals;

namespace {
void PrepareSortedMine(UserQueryResponse& response) {
  erase_if(response.mUserGroups, [](const UserGroup& group) {
    return !group.mName.starts_with("My");
  });
  erase_if(response.mUsers, [](const QRUser& user) {
    return find_if(user.mUids, [](const std::string& uid) { return uid.starts_with("My"); }) == user.mUids.end();
  });
  for (QRUser& user : response.mUsers) {
    sort(user.mGroups);
    sort(user.mUids);
  }
  sort(response.mUserGroups);
  sort(response.mUsers);
}

/*
The following tests attempt to test the basic interactions with the database behind AccessManager::Backend::Storage. At times, there exists a depecdency on other functionality. For example, to
test whether or not a participant has been correctly added to a participantGroup, we depend on createParticipantGroup(), and hasParticipantInGroup().
At this moment, I see no way around this.*/

class AccessManagerStorageTest : public ::testing::Test {
public:
  static std::shared_ptr<AccessManager::Backend::Storage> storage;
  const std::filesystem::path databasePath{"./testDB.sql"};
  static std::shared_ptr<GlobalConfiguration> globalConf;

  const PolymorphicPseudonym dummyPP{PolymorphicPseudonym::FromIdentifier(ElgamalPublicKey::Random(), "dummy")};


  static void SetUpTestSuite() {
    globalConf = std::make_shared<GlobalConfiguration>(Serialization::FromJsonString<GlobalConfiguration>(tests::TEST_SUITE_GLOBAL_CONFIGURATION));
  }

  // Create a new AccessManager::Backend::Storage with a clean database
  void SetUp() override {
    std::filesystem::remove(databasePath);
    storage = std::make_shared<AccessManager::Backend::Storage>(databasePath, globalConf);

  }

  void TearDown() override {
    std::filesystem::remove(databasePath);
  }

  void createParticipantGroupParticipant(const std::string& participantGroup, const LocalPseudonym& localPseudonym) {
    storage->createParticipantGroup(participantGroup);
    // Normally the LocalPseudonym and PolymorphicPseudonym should be linked. For the purposes of these tests this is not required.
    storage->storeLocalPseudonymAndPP(localPseudonym, dummyPP);
    storage->addParticipantToGroup(localPseudonym, participantGroup);
  }

};

std::shared_ptr<AccessManager::Backend::Storage> AccessManagerStorageTest::storage;
std::shared_ptr<GlobalConfiguration> AccessManagerStorageTest::globalConf;

void WaitForNewTimestamp() {
  const Timestamp before = TimeNow();
  while (TimeNow() == before) {
    std::this_thread::sleep_for(1ms);
  }
}

TEST_F(AccessManagerStorageTest, addParticipantToGroup_happy) {
  const std::string pg_name{"newParticipantGroup"};
  const LocalPseudonym localPseudonym{LocalPseudonym::Random()};
  createParticipantGroupParticipant(pg_name, localPseudonym);
  ASSERT_TRUE(storage->hasParticipantInGroup(localPseudonym, pg_name));
}

TEST_F(AccessManagerStorageTest, addParticipantToGroup_non_existent_pg) {
  // Arrange
  const std::string pg_name{"newParticipantGroup"};

  const LocalPseudonym localPseudonym{LocalPseudonym::Random()};
  // Normally the LocalPseudonym and PolymorphicPseudonym should be linked. For the purposes of this test this is not required.
  storage->storeLocalPseudonymAndPP(localPseudonym, dummyPP);

  // Act
  try {
    storage->addParticipantToGroup(localPseudonym, pg_name);
    FAIL() << "This should not have run without exceptions.";
  }
  catch (const Error& e) {
    // Assert
    std::string expectedMessage = "No such participant-group: \"" + pg_name + "\"";
    ASSERT_EQ(e.what(), expectedMessage);
  }

  // Assert
  ASSERT_FALSE(storage->hasParticipantInGroup(localPseudonym, pg_name));
}

TEST_F(AccessManagerStorageTest, addParticipantToGroup_non_existent_pp) {
  // Arrange
  const std::string pg_name{"newParticipantGroup"};
  storage->createParticipantGroup(pg_name);

  const LocalPseudonym localPseudonym{LocalPseudonym::Random()};
  // Act
  try {
    storage->addParticipantToGroup(localPseudonym, pg_name);
    FAIL() << "This should not have run without exceptions.";
  }
  catch (const Error& e) {
    // Assert
    std::string expectedMessage = "No such participant known";
    ASSERT_EQ(e.what(), expectedMessage);
  }

  // Assert
  ASSERT_FALSE(storage->hasParticipantInGroup(localPseudonym, pg_name));
}

TEST_F(AccessManagerStorageTest, addParticipantToGroup_particpant_already_in_pg) {
  // Arrange
  const std::string pg_name{"newParticipantGroup"};
  const LocalPseudonym localPseudonym{LocalPseudonym::Random()};
  createParticipantGroupParticipant(pg_name, localPseudonym);

  // Act
  try {
    storage->addParticipantToGroup(localPseudonym, pg_name); // The pp is already in the pg.
    FAIL() << "This should not have run without exceptions.";
  }
  catch (const Error& e) {
    // Assert
    std::string expectedMessage = "Participant is already in participant-group: \"" + pg_name + "\"";
    ASSERT_EQ(e.what(), expectedMessage);
  }

  // Assert
  ASSERT_TRUE(storage->hasParticipantInGroup(localPseudonym, pg_name));
}

TEST_F(AccessManagerStorageTest, removeParticipantFromGroup_happy) {
  // Arrange
  const std::string pg_name{"newParticipantGroup"};
  const LocalPseudonym localPseudonym{LocalPseudonym::Random()};
  createParticipantGroupParticipant(pg_name, localPseudonym);

  // Act
  storage->removeParticipantFromGroup(localPseudonym, pg_name);

  // Assert
  ASSERT_FALSE(storage->hasParticipantInGroup(localPseudonym, pg_name));
}

TEST_F(AccessManagerStorageTest, removeParticipantFromGroup_particpant_not_in_pg) {
  // Arrange
  const std::string pg_name{"newParticipantGroup"};
  const LocalPseudonym localPseudonym{LocalPseudonym::Random()};
  storage->createParticipantGroup(pg_name);

  // Act
  try {
    storage->removeParticipantFromGroup(localPseudonym, pg_name); // The pp is already in the pg.
    FAIL() << "This should not have run without exceptions.";
  }
  catch (const Error& e) {
    // Assert
    std::string expectedMessage = "This participant is not part of participant-group \"" + pg_name + "\"";
    ASSERT_EQ(e.what(), expectedMessage);
  }

  // Assert
  ASSERT_FALSE(storage->hasParticipantInGroup(localPseudonym, pg_name));
}

TEST_F(AccessManagerStorageTest, createParticipantGroupAccessRule_happy) {
  // Arrange
  std::string userGroup{"Some User"};
  std::string mode{"access"};

  const std::string pg_name{"newParticipantGroup"};
  const LocalPseudonym localPseudonym{LocalPseudonym::Random()};
  createParticipantGroupParticipant(pg_name, localPseudonym);

  // Act
  storage->createParticipantGroupAccessRule(pg_name, userGroup, mode);

  // Assert
  ASSERT_TRUE(storage->hasParticipantGroupAccessRule(pg_name, userGroup, mode));
}

TEST_F(AccessManagerStorageTest, store_lp_and_localPseudonymIsStored) {
  // This test covers 4 endpoints.
  // localPseudonymIsStored(LocalPseudonym)
  // storeLocalPseudonymAndPP(LocalPseudonym, PolymorphicPseudonym)
  // getPPs()
  // getPPs(std::vector<std::string>)
  // Arrange
  const LocalPseudonym localPseudonym{LocalPseudonym::Random()};
  ASSERT_FALSE(storage->hasLocalPseudonym(localPseudonym));
  auto cachedPPsBefore = storage->getPPs();
  auto cachedStarPPsBefore = storage->getPPs({"*"});
  // Act
  storage->storeLocalPseudonymAndPP(localPseudonym, dummyPP);

  // Assert
  ASSERT_TRUE(storage->hasLocalPseudonym(localPseudonym));
  auto cachedPPsAfter = storage->getPPs();
  auto cachedStarPPsAfter = storage->getPPs({"*"});

  // PolymorphicPseudonyms (ElgamalEncryptions) can not be tested on equality. Therefore, test vector length.
  ASSERT_TRUE(cachedPPsAfter.size() - cachedPPsBefore.size() == 1U);
  ASSERT_TRUE(cachedStarPPsAfter.size() - cachedStarPPsBefore.size() == 1U);
}

TEST_F(AccessManagerStorageTest, getStoragePath_happy) {
  auto actual = storage->getPath();
  auto expected = std::filesystem::path(databasePath);
  ASSERT_EQ(actual, expected);
}

TEST_F(AccessManagerStorageTest, getChecksumChainNames_happy) {
  auto actual = storage->getChecksumChainNames();
  auto expected = std::vector<std::string>{
    "column-group-accessrule",
    "column-group-columns",
    "column-groups",
    "columns",
    "group-accessrule",
    "participant-group-participants",
    "participant-group-participants-v2",
    "participant-groups",
    "select-start-pseud",
    "select-start-pseud-v2",
    "user-ids",
    "user-groups",
    "user-group-users-legacy",
    "user-group-users",
    "structure-metadata",
  };
  std::sort(actual.begin(), actual.end());
  std::sort(expected.begin(), expected.end());
  ASSERT_EQ(actual, expected);
}

TEST_F(AccessManagerStorageTest, computeChecksum_unknown_chain) {
  std::string chain{"unknown_chain"};
  std::optional<size_t> maxCheckpoint{};
  uint64_t checksum{};
  uint64_t checkpoint{};
  try {
    storage->computeChecksum(chain, maxCheckpoint, checksum, checkpoint);

    FAIL() << "This should not have run without exceptions.";
  }
  catch (const Error& e) {
    // Assert
    std::string expectedMessage = "No such checksum chain";
    ASSERT_EQ(e.what(), expectedMessage);
  }
}

TEST_F(AccessManagerStorageTest, getColumns_happy) {
  Timestamp timestamp = TimeNow();
  auto actual = storage->getColumns(timestamp);

  size_t expected{58};
  ASSERT_EQ(actual.size(), expected);
}

TEST_F(AccessManagerStorageTest, getColumns_deleted_column) {
  storage->removeColumn("DeviceHistory");
  Timestamp timestamp = TimeNow();
  auto actual = storage->getColumns(timestamp);
  size_t expected{57};
  ASSERT_EQ(actual.size(), expected);
}

TEST_F(AccessManagerStorageTest, getColumnGroups_happy) {
  Timestamp timestamp = TimeNow();
  auto actual = storage->getColumnGroups(timestamp);

  size_t expected{12};
  ASSERT_EQ(actual.size(), expected);
}

TEST_F(AccessManagerStorageTest, getColumnGroups_deleted_columnGroup) {
  storage->removeColumnGroup("Device", true);
  Timestamp timestamp = TimeNow();
  auto actual = storage->getColumnGroups(timestamp);

  size_t expected{11};
  ASSERT_EQ(actual.size(), expected);
}

TEST_F(AccessManagerStorageTest, getColumnGroupColumns_happy) {
  Timestamp timestamp = TimeNow();
  auto actual_beta = storage->getColumnGroupColumns(timestamp);

  size_t expected{133};
  ASSERT_EQ(actual_beta.size(), expected);
}

TEST_F(AccessManagerStorageTest, getColumnGroupColumns_deleted_columnGroup) {
  storage->removeColumnGroup("Device", true);
  Timestamp timestamp = TimeNow();
  auto actual_beta = storage->getColumnGroupColumns(timestamp);

  size_t expected{131};
  ASSERT_EQ(actual_beta.size(), expected);
}

TEST_F(AccessManagerStorageTest, getColumnGroupColumns_deleted_column) {
  Timestamp timestampBefore = TimeNow();
  auto actualBefore = storage->getColumnGroupColumns(timestampBefore);

  storage->removeColumn("DeviceHistory"); // removed from "Device" ColumnGroup and "*" ColumnGroup, 2 removals

  Timestamp timestampAfter = TimeNow();
  auto actualAfter = storage->getColumnGroupColumns(timestampAfter);

  ASSERT_EQ(actualBefore.size() - actualAfter.size(), 2);
}

TEST_F(AccessManagerStorageTest, newUserGetsNewInternalId) {
  std::unordered_set<int64_t> createdIds;
  for(size_t i = 0; i < 10; i++) {
    auto newId = storage->createUser("user" + std::to_string(i));
    auto [iterator, inserted] = createdIds.emplace(newId);
    EXPECT_TRUE(inserted);
  }
}

TEST_F(AccessManagerStorageTest, createUserUidMustBeUnique) {
  storage->createUser("firstname.lastname@pepumc.com"); // typical email as identifier

  EXPECT_ANY_THROW(storage->createUser("firstname.lastname@pepumc.com")); // exactly the same
  EXPECT_ANY_THROW(storage->createUser("Firstname.Lastname@pepumc.com")); // only casing is different
}

TEST_F(AccessManagerStorageTest, findInternalUserId) {
  const auto originalId = storage->createUser("First.Last@pepumc.com"); // typical email as identifier
  storage->createUser("another.user@pepumc.com");

  EXPECT_EQ(storage->findInternalUserId("First.Last@pepumc.com"), originalId); // exact match
  EXPECT_EQ(storage->findInternalUserId("first.last@pepumc.com"), originalId); // different casing
  EXPECT_EQ(storage->findInternalUserId("NotExisting"), std::nullopt);
}

TEST_F(AccessManagerStorageTest, multipleUserIdentifiers) {
  int64_t originalId = storage->createUser("user");
  storage->createUser("anotherUser");
  storage->addIdentifierForUser(originalId, "firstAlternativeName");
  storage->addIdentifierForUser(originalId, "secondAlternativeName");
  EXPECT_EQ(storage->findInternalUserId("firstAlternativeName"), originalId);
  EXPECT_EQ(storage->findInternalUserId("secondAlternativeName"), originalId);
  storage->removeIdentifierForUser(originalId, "secondAlternativeName");
  EXPECT_EQ(storage->findInternalUserId("firstAlternativeName"), originalId);
  EXPECT_EQ(storage->findInternalUserId("secondAlternativeName"), std::nullopt);

  storage->removeUser("user");
  EXPECT_EQ(storage->findInternalUserId("user"), std::nullopt);
  EXPECT_EQ(storage->findInternalUserId("firstAlternativeName"), std::nullopt);
}

TEST_F(AccessManagerStorageTest, cannotRemoveLastUserIdentifier) {
  int64_t originalId = storage->createUser("user");
  storage->addIdentifierForUser(originalId, "firstAlternativeName");
  storage->addIdentifierForUser(originalId, "secondAlternativeName");
  storage->removeIdentifierForUser(originalId, "firstAlternativeName");
  storage->removeIdentifierForUser(originalId, "user");
  EXPECT_ANY_THROW(storage->removeIdentifierForUser(originalId, "secondAlternativeName"));
}

TEST_F(AccessManagerStorageTest, cannotRemoveUidStillInGroups) {
  int64_t originalId = storage->createUser("user");
  storage->createUserGroup(UserGroup("group1", {}));
  storage->addUserToGroup(originalId, "group1");
  EXPECT_ANY_THROW(storage->removeUser(originalId));
  storage->removeUserFromGroup(originalId, "group1");
  EXPECT_NO_THROW(storage->removeUser(originalId));
}

TEST_F(AccessManagerStorageTest, userInGroup_can_add_and_remove_user_from_group) {
  // This test exposed the bug where userInGroup incorrectly uses UserGroupRecord instead of UserGroupUserRecord
  int64_t userId = storage->createUser("testuser");
  storage->createUserGroup(UserGroup("TestGroup", {}));
  storage->addUserToGroup(userId, "TestGroup");
  EXPECT_TRUE(storage->userInGroup("testuser", "TestGroup"));
  storage->removeUserFromGroup(userId, "TestGroup");
  EXPECT_FALSE(storage->userInGroup("testuser", "TestGroup"));
  storage->addUserToGroup(userId, "TestGroup");
  EXPECT_TRUE(storage->userInGroup("testuser", "TestGroup"));
}

TEST_F(AccessManagerStorageTest, userGroupIsEmpty) {
  const std::string group = "MyGroup";
  storage->createUserGroup(UserGroup(group, {}));
  auto userGroupId = storage->getUserGroupId(group);
  EXPECT_TRUE(storage->userGroupIsEmpty(userGroupId));
  const std::string user = "MyUser";
  storage->createUser(user);
  storage->addUserToGroup(user, group);
  EXPECT_FALSE(storage->userGroupIsEmpty(userGroupId));
}

TEST_F(AccessManagerStorageTest, newUserGroupGetsNewUserGroupId) {
  std::unordered_set<int64_t> createdIds;
  for(size_t i = 0; i < 10; i++) {
    auto newId = storage->createUserGroup(UserGroup("group" + std::to_string(i), {}));
    auto [iterator, inserted] = createdIds.emplace(newId);
    EXPECT_TRUE(inserted);
  }
}

TEST_F(AccessManagerStorageTest, findUserGroupId) {
  const UserGroup
      group1 = UserGroup("MyGroup1", {}),
      group2 = UserGroup("MyGroup2", {});

  int64_t group1_id = storage->createUserGroup(group1);
  int64_t group2_id = storage->createUserGroup(group2);

  EXPECT_EQ(storage->findUserGroupId(group1.mName), group1_id);
  EXPECT_EQ(storage->findUserGroupId(group2.mName), group2_id);
}

TEST_F(AccessManagerStorageTest, findUserGroupId_non_existing) {
  const UserGroup
      group1 = UserGroup("MyGroup1", {}),
      group2 = UserGroup("MyGroup2", {});

  storage->createUserGroup(group1);
  storage->createUserGroup(group2);

  EXPECT_EQ(storage->findUserGroupId("NotExisting"), std::nullopt);
}

TEST_F(AccessManagerStorageTest, findUserGroupId_with_changed_validity) {
  UserGroup
      group1 = UserGroup("MyGroup1", {}),
      group2 = UserGroup("MyGroup2", {});

  int64_t group1_id = storage->createUserGroup(group1);
  int64_t group2_id = storage->createUserGroup(group2);
  group1.mMaxAuthValidity = 42s;
  storage->modifyUserGroup(group1);

  EXPECT_EQ(storage->findUserGroupId(group1.mName), group1_id);
  EXPECT_EQ(storage->findUserGroupId(group2.mName), group2_id);
}

// ==== executeQuery ====

TEST_F(AccessManagerStorageTest, executeQuery_unfiltered_groups) {
  const UserGroup
      group1 = UserGroup("MyGroup1", 42s),
      group2 = UserGroup("MyGroup2", {});

  storage->createUserGroup(group1);
  storage->createUserGroup(group2);

  auto response = storage->executeUserQuery({TimeNow(), "", ""});
  PrepareSortedMine(response);
  const auto groupNames = RangeToVector(response.mUserGroups | views::transform(std::mem_fn(&UserGroup::mName)));
  EXPECT_EQ(groupNames, (std::vector{group1.mName, group2.mName})) << "should return all group names";
  EXPECT_EQ(response.mUserGroups, (std::vector<UserGroup>{
      group1,
      group2,
    })) << "should return all group properties";
}

TEST_F(AccessManagerStorageTest, executeQuery_unfiltered_users) {
  const std::string
      user1 = "MyUser1",
      user2 = "MyUser2";
  storage->createUser(user1);
  storage->createUser(user2);

  auto response = storage->executeUserQuery({TimeNow(), "", ""});
  PrepareSortedMine(response);
  EXPECT_EQ(response.mUsers, (std::vector<QRUser>{
      {{user1}, {}},
      {{user2}, {}},
    })) << "should return all users";
}

TEST_F(AccessManagerStorageTest, executeQuery_unfiltered_users_alt_ids) {
  const std::string
      user1 = "MyUser1",
      user1Alt = "MyUser1-alt";
  storage->createUser(user1);
  storage->addIdentifierForUser(user1, user1Alt);

  auto response = storage->executeUserQuery({TimeNow(), "", ""});
  PrepareSortedMine(response);
  EXPECT_EQ(response.mUsers, (std::vector<QRUser>{
      {{user1, user1Alt}, {}},
    })) << "should return alternative identifiers";
}

TEST_F(AccessManagerStorageTest, executeQuery_unfiltered_group_memberships) {
  const std::string
      group1 = "MyGroup1",
      group2 = "MyGroup2";
  const std::string
      user1 = "MyUser1",
      user1Alt = "MyUser1-alt",
      user2 = "MyUser2";

  storage->createUserGroup(UserGroup(group1, {}));
  storage->createUserGroup(UserGroup(group2, {}));

  storage->createUser(user1);
  storage->addIdentifierForUser(user1, user1Alt);
  storage->createUser(user2);

  storage->addUserToGroup(user1, group1);
  storage->addUserToGroup(user2, group2);

  auto response = storage->executeUserQuery({TimeNow(), "", ""});
  PrepareSortedMine(response);
  EXPECT_EQ(response.mUsers, (std::vector<QRUser>{
      {{user1, user1Alt}, {group1}},
      {{user2}, {group2}},
    })) << "should return user-group memberships";
}

TEST_F(AccessManagerStorageTest, executeQuery_filtered_group) {
  const std::string
      group1 = "MyGroup1",
      group2 = "MyGroup2";
  const std::string
      user1 = "MyUser1",
      user1Alt = "MyUser1-alt",
      user2 = "MyUser2",
      user3 = "MyUser3@both-groups";

  storage->createUserGroup(UserGroup(group1, {}));
  storage->createUserGroup(UserGroup(group2, {}));

  storage->createUser(user1);
  storage->addIdentifierForUser(user1, user1Alt);
  storage->createUser(user2);
  storage->createUser(user3);

  storage->addUserToGroup(user1, group1);
  storage->addUserToGroup(user2, group2);
  storage->addUserToGroup(user3, group1);
  storage->addUserToGroup(user3, group2);

  auto response = storage->executeUserQuery({TimeNow(), "Group1", ""});
  PrepareSortedMine(response);

  const auto groupNames = RangeToVector(response.mUserGroups | views::transform(std::mem_fn(&UserGroup::mName)));
  EXPECT_EQ(groupNames, std::vector{group1}) << "should return filtered group names";

  EXPECT_EQ(response.mUsers, (std::vector<QRUser>{
      {{user1, user1Alt}, {group1}},
      {{user3}, {group1}}, // Note: we don't return group2 for user3
    })) << "should return group-filtered users with group memberships";
}

TEST_F(AccessManagerStorageTest, executeQuery_filtered_user) {
  const std::string
      group1 = "MyGroup1",
      group2 = "MyGroup2";
  const std::string
      user1 = "MyUser1",
      user1Alt = "MyUser1-alt",
      user2 = "MyUser2",
      user3 = "MyUser3@both-groups";

  storage->createUserGroup(UserGroup(group1, {}));
  storage->createUserGroup(UserGroup(group2, {}));

  storage->createUser(user1);
  storage->addIdentifierForUser(user1, user1Alt);
  storage->createUser(user2);
  storage->createUser(user3);

  storage->addUserToGroup(user1, group1);
  storage->addUserToGroup(user2, group2);
  storage->addUserToGroup(user3, group1);
  storage->addUserToGroup(user3, group2);

  auto response = storage->executeUserQuery({TimeNow(), "", "User1"});
  PrepareSortedMine(response);

  EXPECT_EQ(response.mUsers, (std::vector<QRUser>{
      {{user1, user1Alt}, {group1}}, // Note: we also want to see alternative IDs
    })) << "should return filtered users with all alt IDs with group memberships";

  const auto groupNames = RangeToVector(response.mUserGroups | views::transform(std::mem_fn(&UserGroup::mName)));
  EXPECT_EQ(groupNames, std::vector{group1}) << "should return user-filtered group names";
}

TEST_F(AccessManagerStorageTest, executeQuery_filtered_user_alt) {
  const std::string
      group1 = "MyGroup1",
      group2 = "MyGroup2";
  const std::string
      user1 = "MyUser1",
      user1Alt = "MyUser1-alt",
      user2 = "MyUser2";

  storage->createUserGroup(UserGroup(group1, {}));
  storage->createUserGroup(UserGroup(group2, {}));

  storage->createUser(user1);
  storage->addIdentifierForUser(user1, user1Alt);
  storage->createUser(user2);

  storage->addUserToGroup(user1, group1);
  storage->addUserToGroup(user2, group2);

  auto response = storage->executeUserQuery({TimeNow(), "", "-alt"});
  PrepareSortedMine(response);
  EXPECT_EQ(response.mUsers, (std::vector<QRUser>{
      {{user1, user1Alt}, {group1}},
    })) << "should return filtered users with all alt IDs with group memberships";

  const auto groupNames = RangeToVector(response.mUserGroups | views::transform(std::mem_fn(&UserGroup::mName)));
  EXPECT_EQ(groupNames, std::vector{group1}) << "should return user-filtered group names";
}

TEST_F(AccessManagerStorageTest, executeQuery_filtered_user_and_group) {
  const std::string
      groupA1 = "MyGroupA1",
      groupA2 = "MyGroupA2",
      groupB1 = "MyGroupB1",
      groupB2 = "MyGroupB2";
  const std::string
      userA1 = "MyUserA1",
      userA2 = "MyUserA2",
      userB1 = "MyUserB1",
      userB2 = "MyUserB2";

  for (auto group : {groupA1, groupA2, groupB1, groupB2}) {
    storage->createUserGroup(UserGroup(std::move(group), {}));
  }
  for (auto user : {userA1, userA2, userB1, userB2}) {
    storage->createUser(std::move(user));
  }

  storage->addUserToGroup(userA1, groupA1);
  storage->addUserToGroup(userA2, groupB1);
  storage->addUserToGroup(userB1, groupA1);
  storage->addUserToGroup(userB2, groupA2);

  storage->addUserToGroup(userA1, groupB1);

  auto response = storage->executeUserQuery({TimeNow(), "GroupA", "UserA"});
  PrepareSortedMine(response);
  EXPECT_EQ(response.mUsers, (std::vector<QRUser>{
      {{userA1}, {groupA1}},
    })) << "should return double-filtered users with group memberships";

  const auto groupNames = RangeToVector(response.mUserGroups | views::transform(std::mem_fn(&UserGroup::mName)));
  EXPECT_EQ(groupNames, std::vector{groupA1}) << "should return double-filtered group names";
}

// ====

using MetadataMap = std::map<std::string /*subject*/, std::map<StructureMetadataKey, std::string /*value*/>>;

namespace {

MetadataMap MetadataToMap(std::vector<StructureMetadataEntry> vec) {
  MetadataMap map;
  for (StructureMetadataEntry& entry : vec) {
    auto& subjectMeta = map[std::move(entry.subjectKey.subject)];
    auto emplaced = subjectMeta.emplace(std::move(entry.subjectKey.key), std::move(entry.value));
    if (!emplaced.second) {
      throw std::runtime_error("Found multiple entries with same key");
    }
  }
  return map;
}

} // namespace

TEST_F(AccessManagerStorageTest, setGetMetadataBasicColumn) {
  const std::string groupA = "meta_groupA";
  const StructureMetadataKey key1{groupA, "meta_subkey1"};
  const std::string value1 = "Cool metadata value 1!";

  EXPECT_THROW(storage->setStructureMetadata(StructureMetadataType::Column, "NonExistingColumn", key1, value1),
      Error) << "setStructureMetadata should refuse non-existing column";

  const std::string columnName = "ColumnWithMetadata";
  storage->createColumn(columnName);
  {
    {
      const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), StructureMetadataType::Column));
      EXPECT_FALSE(metaMap.contains(columnName))
          << "getStructureMetadata should exclude column without metadata, but returns a map with " << metaMap.at(columnName).size() << " entries";
    }

    ASSERT_NO_THROW(storage->setStructureMetadata(StructureMetadataType::Column, columnName, key1, value1));
    {
      const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), StructureMetadataType::Column));
      EXPECT_TRUE(metaMap.contains(columnName)) << "getStructureMetadata should return column for just-added metadata";
      const auto& colMetaMap = metaMap.at(columnName);
      EXPECT_TRUE(colMetaMap.contains(key1)) << "getStructureMetadata should return key of just-added metadata";
      EXPECT_EQ(colMetaMap.at(key1), value1) << "getStructureMetadata should return value of just-added metadata";
    }

    const StructureMetadataKey key2{groupA, "meta_subkey2"};
    const std::string value2 = "Cool metadata value 2!";
    ASSERT_NO_THROW(storage->setStructureMetadata(StructureMetadataType::Column, columnName, key2, value2))
        << "setStructureMetadata should be able to add multiple entries";
    {
      const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), StructureMetadataType::Column, {.subjects = {columnName}}));
      MetadataMap expected{{columnName, {{key1, value1}, {key2, value2}}}};
      EXPECT_EQ(metaMap, expected) << "getStructureMetadata should retrieve multiple entries";
    }

    const std::string groupB = "meta_groupB";
    const StructureMetadataKey keyB1{"meta_groupB", "meta_subkey1"};
    const std::string valueB1 = "Cool metadata value B1!";
    storage->setStructureMetadata(StructureMetadataType::Column, columnName, keyB1, valueB1);

    {
      const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), StructureMetadataType::Column, {.subjects = {columnName}, .keys = {key2}}));
      MetadataMap expected{{columnName, {{key2, value2}}}};
      EXPECT_EQ(metaMap, expected) << "getStructureMetadata should filter by a single key";
    }
    {
      const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), StructureMetadataType::Column, {.subjects = {columnName}, .keys = {key1, keyB1}}));
      MetadataMap expected{{columnName, {{key1, value1}, {keyB1, valueB1}}}};
      EXPECT_EQ(metaMap, expected) << "getStructureMetadata should filter by multiple keys";
    }
    {
      const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), StructureMetadataType::Column, {.subjects = {columnName}, .keys = {{groupA, ""}}}));
      MetadataMap expected{{columnName, {{key1, value1}, {key2, value2}}}};
      EXPECT_EQ(metaMap, expected) << "getStructureMetadata should filter by metadata group";
    }
    {
      const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), StructureMetadataType::Column, {.subjects = {columnName}, .keys = {key1, {groupB, ""}}}));
      MetadataMap expected{{columnName, {{key1, value1}, {keyB1, valueB1}}}};
      EXPECT_EQ(metaMap, expected) << "getStructureMetadata should filter by metadata group or key";
    }
  }

  const std::string columnName2 = "ColumnWithMetadata2";
  storage->createColumn(columnName2);
  storage->setStructureMetadata(StructureMetadataType::Column, columnName2, key1, value1);

  {
    const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), StructureMetadataType::Column, {.keys = {key1}}));
    MetadataMap expected{{columnName, {{key1, value1}}}, {columnName2, {{key1, value1}}}};
    EXPECT_EQ(metaMap, expected) << "getStructureMetadata should be able to filter by key for multiple columns";
  }
  {
    const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), StructureMetadataType::Column, {.subjects = {columnName}, .keys = {key1}}));
    MetadataMap expected{{columnName, {{key1, value1}}}};
    EXPECT_EQ(metaMap, expected) << "getStructureMetadata should be able to filter by column";
  }
}

TEST_F(AccessManagerStorageTest, removeMetadata) {
  const std::string
      column1 = "ColumnWithMetadata",
      column2 = "ColumnWithMetadata2";
  storage->createColumn(column1);
  storage->createColumn(column2);

  const std::pair<StructureMetadataKey, std::string /*value*/>
      metaEntry1{{"meta_group", "meta_key"}, "meta value"},
      metaEntry2{{"meta_group", "meta_key2"}, "meta value 2"};

  MetadataMap metaEntries{
      {column1, {metaEntry1, metaEntry2}},
      {column2, {metaEntry1}},
  };
  for (const auto& [column, colEntries] : metaEntries) {
    for (const auto& [key, value] : colEntries) {
      storage->setStructureMetadata(StructureMetadataType::Column, column, key, value);
    }
  }
  {
    const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), StructureMetadataType::Column));
    ASSERT_EQ(metaMap, metaEntries) << "[sanity check] metadata should be added";
  }

  storage->removeStructureMetadata(StructureMetadataType::Column, column1, metaEntry1.first);
  metaEntries.at(column1).erase(metaEntry1.first);
  {
    const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), StructureMetadataType::Column));
    ASSERT_EQ(metaMap, metaEntries) << "removeStructureMetadata should remove a single entry";
  }

  EXPECT_THROW(storage->removeStructureMetadata(StructureMetadataType::Column, column1, metaEntry1.first),
      Error) << "removeStructureMetadata should refuse to re-delete";

  EXPECT_THROW(storage->removeStructureMetadata(StructureMetadataType::Column, "NonExistingColumn", metaEntry1.first),
      Error) << "removeStructureMetadata should refuse to delete for non-existing column";
}

TEST_F(AccessManagerStorageTest, removeMetadataStructure) {
  const std::string structure = "StructureWithMetadata";

  const StructureMetadataKey key{"meta_group", "meta_key"};
  const std::string value = "meta value";

  struct context {
    StructureMetadataType structureType;
    std::string description;
    std::function<void()> createStructure, removeStructure;
  };
  auto contexts = std::initializer_list<context>{
      {StructureMetadataType::Column, "column", [&] {storage->createColumn(structure); }, [&] {storage->removeColumn(structure); }},
      {StructureMetadataType::ColumnGroup, "column group", [&] {storage->createColumnGroup(structure); },
          [&] {storage->removeColumnGroup(structure, false); }},
      {StructureMetadataType::ParticipantGroup, "participant group", [&] {storage->createParticipantGroup(structure); },
          [&] {storage->removeParticipantGroup(structure, false); }},
  };
  for (const context& ctx : contexts) {
    ctx.createStructure();
    storage->setStructureMetadata(ctx.structureType, structure, key, value);
    {
      const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), ctx.structureType));
      MetadataMap expected{{structure, {{key, value}}}};
      ASSERT_EQ(metaMap, expected) << "[sanity check] metadata should be added to " << ctx.description;
    }

    ctx.removeStructure();
    {
      const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), ctx.structureType));
      EXPECT_EQ(metaMap, MetadataMap{}) << "metadata should be removed when removing " << ctx.description;
    }

    ctx.createStructure();
    {
      const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), ctx.structureType));
      EXPECT_EQ(metaMap, MetadataMap{}) << "metadata should stay removed when re-creating " << ctx.description;
    }
  }
}

TEST_F(AccessManagerStorageTest, setMetadataOverwrite) {
  const std::string column = "ColumnWithMetadata";
  storage->createColumn(column);

  const StructureMetadataKey key{"meta_group", "meta_key"};
  const std::string
      value1 = "meta value 1",
      value2 = "meta value 2";

  MetadataMap metaEntries{{column, {{key, value1}}}};
  for (const auto& [column, colEntries] : metaEntries) {
    for (const auto& [key, value] : colEntries) {
      storage->setStructureMetadata(StructureMetadataType::Column, column, key, value);
    }
  }
  {
    const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), StructureMetadataType::Column));
    ASSERT_EQ(metaMap, metaEntries) << "[sanity check] metadata should be added";
  }

  storage->setStructureMetadata(StructureMetadataType::Column, column, key, value2);
  metaEntries.at(column).at(key) = value2;
  {
    const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), StructureMetadataType::Column));
    ASSERT_EQ(metaMap, metaEntries) << "setStructureMetadata should overwrite an entry";
  }
}

TEST_F(AccessManagerStorageTest, setMetadataBinary) {
  const std::string column = "ColumnWithMetadata";
  storage->createColumn(column);

  const StructureMetadataKey key{"meta_group", "meta_key"};
  // Make sure to include the \0 in the string by using a std::string literal
  const auto value = "meta value \0\1\2\3\4\5\6"s;

  storage->setStructureMetadata(StructureMetadataType::Column, column, key, value);
  {
    const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), StructureMetadataType::Column));
    MetadataMap expected{{column, {{key, value}}}};
    EXPECT_EQ(metaMap, expected) << "metadata should support binary values";
  }
}

TEST_F(AccessManagerStorageTest, getSetRemoveMetadataSeparateSubjectType) {
  const StructureMetadataKey key{"meta_group", "meta_key"};
  const std::string value = "meta value";

  const std::string subject = "ColumnWithMetadata";
  storage->createColumn(subject);
  storage->createColumnGroup(subject);

  const MetadataMap metaEntries{{subject, {{key, value}}}};

  storage->setStructureMetadata(StructureMetadataType::Column, subject, key, value);
  {
    const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), StructureMetadataType::Column));
    ASSERT_EQ(metaMap, metaEntries) << "[sanity check] metadata should be added";
  }
  {
    const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), StructureMetadataType::ColumnGroup));
    EXPECT_EQ(metaMap, MetadataMap{}) << "metadata should not be added to / retrieved from wrong metadata subject type";
  }

  storage->setStructureMetadata(StructureMetadataType::ColumnGroup, subject, key, value);
  {
    const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), StructureMetadataType::ColumnGroup));
    ASSERT_EQ(metaMap, metaEntries) << "[sanity check] metadata should be added";
  }

  storage->removeStructureMetadata(StructureMetadataType::Column, subject, key);
  {
    const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), StructureMetadataType::Column));
    ASSERT_EQ(metaMap, MetadataMap{}) << "[sanity check] metadata should be removed";
  }
  {
    const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), StructureMetadataType::ColumnGroup));
    ASSERT_EQ(metaMap, metaEntries) << "removeStructureMetadata should not remove metadata from wrong metadata subject type";
  }
}

TEST_F(AccessManagerStorageTest, setGetMetadataColumnGroup) {
  constexpr StructureMetadataType subjectType = StructureMetadataType::ColumnGroup;

  const StructureMetadataKey key{"meta_group", "meta_key"};
  const std::string value = "meta value";
  EXPECT_THROW(storage->setStructureMetadata(subjectType, "NonExisting", key, value),
      Error) << "setStructureMetadata should refuse non-existing column group";
  const std::string subject = "ColumnGroupWithMetadata";
  storage->createColumnGroup(subject);

  ASSERT_NO_THROW(storage->setStructureMetadata(subjectType, subject, key, value));
  {
    const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), subjectType));
    MetadataMap expected{{subject, {{key, value}}}};
    ASSERT_EQ(metaMap, expected) << "metadata should be added";
  }
}

TEST_F(AccessManagerStorageTest, setGetMetadataParticipantGroup) {
  constexpr StructureMetadataType subjectType = StructureMetadataType::ParticipantGroup;

  const StructureMetadataKey key{"meta_group", "meta_key"};
  const std::string value = "meta value";
  EXPECT_THROW(storage->setStructureMetadata(subjectType, "NonExisting", key, value),
      Error) << "setStructureMetadata should refuse non-existing participant group";
  const std::string subject = "ParticipantGroupWithMetadata";
  storage->createParticipantGroup(subject);

  ASSERT_NO_THROW(storage->setStructureMetadata(subjectType, subject, key, value));
  {
    const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), subjectType));
    MetadataMap expected{{subject, {{key, value}}}};
    ASSERT_EQ(metaMap, expected) << "metadata should be added";
  }
}

TEST_F(AccessManagerStorageTest, getMetadataHistoric) {
  const StructureMetadataKey key{"meta_group", "meta_key"};
  const std::string value = "meta value";

  const std::string subject = "ColumnWithMetadata";
  storage->createColumn(subject);

  const MetadataMap metaEntries{{subject, {{key, value}}}};

  storage->setStructureMetadata(StructureMetadataType::Column, subject, key, value);
  {
    const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), StructureMetadataType::Column));
    ASSERT_EQ(metaMap, metaEntries) << "[sanity check] metadata should be added";
  }

  const Timestamp preRemove = TimeNow();
  WaitForNewTimestamp();

  storage->removeStructureMetadata(StructureMetadataType::Column, subject, key);
  {
    const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), StructureMetadataType::Column));
    ASSERT_EQ(metaMap, MetadataMap{}) << "[sanity check] metadata should be removed";
  }
  {
    const auto metaMap = MetadataToMap(storage->getStructureMetadata(preRemove, StructureMetadataType::Column));
    EXPECT_EQ(metaMap, metaEntries) << "getStructureMetadata should retrieve historic data";
  }
}

TEST_F(AccessManagerStorageTest, getMetadataKeys) {
  const StructureMetadataKey key{"meta_group", "meta_key"};
  const std::string value = "meta value";

  const std::string
      subject = "ColumnWithMetadata",
      subject2 = "ColumnWithMetadata2";
  storage->createColumn(subject);
  storage->createColumn(subject2);

  const MetadataMap metaEntries{{subject, {{key, value}}}};

  storage->setStructureMetadata(StructureMetadataType::Column, subject, key, value);
  {
    const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), StructureMetadataType::Column));
    ASSERT_EQ(metaMap, metaEntries) << "[sanity check] metadata should be added";
  }

  {
    const auto metaKeys = storage->getStructureMetadataKeys(TimeNow(), StructureMetadataType::Column, subject);
    ASSERT_EQ(metaKeys, std::vector{key}) << "getStructureMetadataKeys should return just-added keys";
  }
  {
    const auto metaKeys = storage->getStructureMetadataKeys(TimeNow(), StructureMetadataType::Column, subject2);
    ASSERT_EQ(metaKeys, std::vector<StructureMetadataKey>{}) << "getStructureMetadataKeys should not return keys added to other subject";
  }

  const Timestamp preRemove = TimeNow();
  WaitForNewTimestamp();

  storage->removeStructureMetadata(StructureMetadataType::Column, subject, key);
  {
    const auto metaMap = MetadataToMap(storage->getStructureMetadata(TimeNow(), StructureMetadataType::Column));
    ASSERT_EQ(metaMap, MetadataMap{}) << "[sanity check] metadata should be removed";
  }
  {
    const auto metaKeys = storage->getStructureMetadataKeys(TimeNow(), StructureMetadataType::Column, subject);
    ASSERT_EQ(metaKeys, std::vector<StructureMetadataKey>{}) << "getStructureMetadataKeys should not return removed keys";
  }
  {
    const auto metaKeys = storage->getStructureMetadataKeys(preRemove, StructureMetadataType::Column, subject);
    EXPECT_EQ(metaKeys, std::vector{key}) << "getStructureMetadataKeys should retrieve historic data";
  }
}

} // namespace
