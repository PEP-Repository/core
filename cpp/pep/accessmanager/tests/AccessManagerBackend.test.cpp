#include <gtest/gtest.h>
#include <pep/utils/File.hpp>
#include <pep/structure/StructureSerializers.hpp>
#include <pep/accessmanager/tests/TestSuiteGlobalConfiguration.hpp>

#include <pep/accessmanager/Storage.hpp>

#include <filesystem>
#include <gmock/gmock-matchers.h>

using namespace pep;

namespace {
/* This testsuite aims to test all interactions with the AccessManager::Backend that involve logic in the backend layer.
* For any pass through functionality, such as addParticipantToGroup(), see AccessManagerStorage.test.cpp
*
* TODO: The AMAQuery tests are now based on vector sizes of the result. This is a unclear way of testing. It also is dependent on the behaviour of storage.ensureInitialized().
*       Create a more clear and robust way of testing this functionality.
*/

class AccessManagerBackendTest : public ::testing::Test {
public:

  static std::shared_ptr<AccessManager::Backend> backend;
  static std::shared_ptr<AccessManager::Backend::Storage> storage; // Have a direct variable so we can check the storage state directly, without going through backend
  static std::shared_ptr<GlobalConfiguration> globalConf;

  struct User {
    std::string primaryId;
    std::string displayId;
    std::vector<std::string> userGroups;
  };

  struct Constants {
    const std::filesystem::path databasePath{"./testDB.sql"};

    const std::string userGroup1{"TestUserGroup"};
    const std::string userGroup2{"TestUserGroupWithoutAccess"};
    const std::string userGroup3{"TestUserGroupWithoutMembers"};

    const User user1{"123", "TestUserInOneGroup", {userGroup1}};
    const User user2{"456", "TestUserInMultipleGroups", {userGroup1, userGroup2}};
    const User user3{"789", "TestUserInNoGroups", {}};
    const std::vector<User> users{user1, user2, user3};
    const std::string unusedPrimaryId{"abc"};
    const std::string nonExistingUser{"NonExistingUser"};

    const std::string r_col1{"readColumn_1"};
    const std::string r_col2{"readColumn_2"};
    const std::string r_cg1{"readColumnGroup_1"};
    const std::string r_cg2{"readColumnGroup_2"};

    const std::string w_col{"writeColumn_1"};
    const std::string w_cg{"writeColumnGroup"};

    const std::string pg1{"participantGroup_1"};
    const std::string pg2{"participantGroup_2"};

    const std::string rm_col{"readMetaColumn"};
    const std::string rm_cg{"readMetaColumnGroup"};

    const std::string wm_col{"writeMetaColumn"};
    const std::string wm_cg{"writeMetaColumnGroup"};

    const std::string empty_cg{"emptyColumnGroup"}; //cg without any columns in it.

    const std::string star_col{"starColumn"}; // This column will not be in any columnGroup, except for "*".
    const std::string double_col{"doubleColumn"}; // This column is in both ReadColumnGroup1 and WriteColumnGroup, giving the user both access rights, through two paths.

    const LocalPseudonym localPseudonym1{LocalPseudonym::Random()};
    const LocalPseudonym localPseudonym2{LocalPseudonym::Random()};

    const PolymorphicPseudonym dummyPP{PolymorphicPseudonym::FromIdentifier(ElgamalPublicKey::Random(), "dummy")};
  };

  static Constants constants;

  static void SetUpTestSuite() {
    globalConf = std::make_shared<GlobalConfiguration>(Serialization::FromJsonString<GlobalConfiguration>(tests::TEST_SUITE_GLOBAL_CONFIGURATION));
    std::filesystem::remove(constants.databasePath);
    storage = std::make_shared<AccessManager::Backend::Storage>(constants.databasePath, globalConf);
    backend = std::make_shared<AccessManager::Backend>(storage);
    populateDatabase();
  }

  static void TearDownTestSuite() {
    std::filesystem::remove(constants.databasePath);
  }


  // Create a basic administration with a few columngroups and participantgroups defined.
  static void populateDatabase() {
    storage->createUserGroup(UserGroup{constants.userGroup1, {}});
    storage->createUserGroup(UserGroup{constants.userGroup2, {}});
    storage->createUserGroup(UserGroup{constants.userGroup3, {}});

    for (auto& user : constants.users) {
      auto internalId = storage->createUser(user.displayId);
      storage->addIdentifierForUser(internalId, user.primaryId, UserIdFlags::isPrimaryId);
      for (auto& usergroup : user.userGroups) {
        storage->addUserToGroup(internalId, usergroup);
      }
    }

    // Normally the LocalPseudonym and PolymorphicPseudonym should be linked. For the purposes of this test this is not required.
    storage->storeLocalPseudonymAndPP(constants.localPseudonym1, constants.dummyPP);
    storage->storeLocalPseudonymAndPP(constants.localPseudonym2, constants.dummyPP);

    // ParticipantGroup with access and enumerate rights for userGroup
    storage->createParticipantGroup(constants.pg1);
    storage->addParticipantToGroup(constants.localPseudonym1, constants.pg1);
    storage->createParticipantGroupAccessRule(constants.pg1, constants.userGroup1, "access");
    storage->createParticipantGroupAccessRule(constants.pg1, constants.userGroup1, "enumerate");

    // ParticipantGroup without those rights.
    storage->createParticipantGroup(constants.pg2);
    storage->addParticipantToGroup(constants.localPseudonym2, constants.pg2);

    // ColumnGroup with read rights for userGroup
    storage->createColumn(constants.r_col1);
    storage->createColumn(constants.r_col2);
    storage->createColumnGroup(constants.r_cg1);
    storage->createColumnGroup(constants.r_cg2);
    storage->addColumnToGroup(constants.r_col1, constants.r_cg1);
    storage->addColumnToGroup(constants.r_col1, constants.r_cg2); // readColumn1 is in two columnGroups
    storage->addColumnToGroup(constants.r_col2, constants.r_cg1);
    storage->createColumnGroupAccessRule(constants.r_cg1, constants.userGroup1, "read");
    storage->createColumnGroupAccessRule(constants.r_cg2, constants.userGroup1, "read");


    // ColumnGroup with write rights for userGroup
    storage->createColumn(constants.w_col);
    storage->createColumnGroup(constants.w_cg);
    storage->addColumnToGroup(constants.w_col, constants.w_cg);
    storage->createColumnGroupAccessRule(constants.w_cg, constants.userGroup1, "write");

    // ColumnGroup with read-meta rights for userGroup
    storage->createColumn(constants.rm_col);
    storage->createColumnGroup(constants.rm_cg);
    storage->addColumnToGroup(constants.rm_col, constants.rm_cg);
    storage->createColumnGroupAccessRule(constants.rm_cg, constants.userGroup1, "read-meta");

    // ColumnGroup with write-meta rights for userGroup
    storage->createColumn(constants.wm_col);
    storage->createColumnGroup(constants.wm_cg);
    storage->addColumnToGroup(constants.wm_col, constants.wm_cg);
    storage->createColumnGroupAccessRule(constants.wm_cg, constants.userGroup1, "write-meta");

    // ColumnGroup without any columns or cgars
    storage->createColumnGroup(constants.empty_cg);

    // Column that has no columnGroup attached.
    storage->createColumn(constants.star_col);

    // Column with both read and write access, but through two diufferent columngroups
    storage->createColumn(constants.double_col);
    storage->addColumnToGroup(constants.double_col, constants.r_cg1);
    storage->addColumnToGroup(constants.double_col, constants.w_cg);
  }
};

std::shared_ptr<AccessManager::Backend> AccessManagerBackendTest::backend;
AccessManagerBackendTest::Constants AccessManagerBackendTest::constants;
std::shared_ptr<AccessManager::Backend::Storage> AccessManagerBackendTest::storage;
std::shared_ptr<GlobalConfiguration> AccessManagerBackendTest::globalConf;

TEST_F(AccessManagerBackendTest, unfoldColumnGroupsAndAssertAccess_happy) {

  const std::vector<std::string> columngroups{constants.r_cg1};
  const std::vector<std::string> modes{"read"};
  Timestamp timestamp{};
  std::vector<std::string> columns;
  std::unordered_map<std::string, IndexList> columnGroupMap{};

  backend->unfoldColumnGroupsAndAssertAccess(constants.userGroup1, columngroups, modes, timestamp, columns, columnGroupMap);

  std::unordered_map<std::string, IndexList> expectedColumnGroupMap{{constants.r_cg1, IndexList({0, 1, 2})}};
  std::vector<std::string> expectedColumns = {constants.double_col, constants.r_col1, constants.r_col2};

  //checkMapEquality(columnGroupMap, expectedColumnGroupMap);
  EXPECT_EQ(columnGroupMap, expectedColumnGroupMap);

  // Sort calculated and expected values the same way to prevent failure due to ordering differences
  std::sort(columns.begin(), columns.end());
  std::sort(expectedColumns.begin(), expectedColumns.end());

  EXPECT_EQ(columns, expectedColumns);
}

TEST_F(AccessManagerBackendTest, unfoldColumnGroupsAndAssertAccess_column_access_through_multiple_column_groups_no_column_groups_in_request) {
  // The userGroup has read and write acces to the column, but through different columngroups. Access should be granted.
  const std::vector<std::string> columngroups{};
  const std::vector<std::string> modes{"read", "write"};
  Timestamp timestamp{};
  std::vector<std::string> columns{constants.double_col};
  std::unordered_map<std::string, IndexList> columnGroupMap{};
  backend->unfoldColumnGroupsAndAssertAccess(constants.userGroup1, columngroups, modes, timestamp, columns, columnGroupMap);

  std::unordered_map<std::string, IndexList> expectedColumnGroupMap{};
  std::vector<std::string> expectedColumns = {constants.double_col};

  EXPECT_EQ(columnGroupMap, expectedColumnGroupMap);
  EXPECT_EQ(columns, expectedColumns);
}

TEST_F(AccessManagerBackendTest, unfoldColumnGroupsAndAssertAccess_no_column_access_no_column_groups_in_request) {
  // The userGroup has read and write acces to the column, but through different columngroups. Access should be granted.
  const std::vector<std::string> columngroups{};
  const std::vector<std::string> modes{"read", "write"};
  Timestamp timestamp{};
  std::vector<std::string> columns{constants.w_col};
  std::unordered_map<std::string, IndexList> columnGroupMap{};


  try {
    // Act
    backend->unfoldColumnGroupsAndAssertAccess(constants.userGroup1, columngroups, modes, timestamp, columns, columnGroupMap);

    FAIL() << "This should not have run without exceptions.";
  }
  catch (const Error& e) {
    // Assert
    std::string expectedMessage = "Access denied to \"TestUserGroup\" for mode \"read\" to column \"writeColumn_1\"";
    EXPECT_EQ(e.what(), expectedMessage);
  }
}
TEST_F(AccessManagerBackendTest, checkTicketRequest_happy) {
  TicketRequest2 request;
  // Existing participantGroup
  request.mParticipantGroups.push_back(constants.pg1);
  // Not both participantGroups and participants
  request.mPolymorphicPseudonyms = {};
  // Existing columnGroup
  request.mColumnGroups.push_back(constants.w_cg);
  // Existing column
  request.mColumns.push_back(constants.w_col);

  backend->checkTicketRequest(request);
}

TEST_F(AccessManagerBackendTest, checkTicketRequest_fails_on_both_pp_and_pgs) {
  TicketRequest2 request;
  // Existing participantGroup
  request.mParticipantGroups.push_back(constants.pg1);
  // Both participantGroups and participants
  request.mPolymorphicPseudonyms.push_back(constants.dummyPP); // Nonsense pp, the content is irrelevant
  // Existing columnGroup
  request.mColumnGroups.push_back(constants.w_cg);
  // Existing column
  request.mColumns.push_back(constants.w_col);

  try {
    // Act
    backend->checkTicketRequest(request);

    FAIL() << "This should not have run without exceptions.";
  }
  catch (const Error& e) {
    // Assert
    std::string expectedMessage = "The ticket request contains participant group(s) as well as specific participant(s). This is not supported. Use either groups or specific participants.";
    EXPECT_EQ(e.what(), expectedMessage);
  }
}

TEST_F(AccessManagerBackendTest, checkTicketRequest_fails_on_non_existing_pg_cg_and_col) {
  TicketRequest2 request;
  // Existing participantGroup
  request.mParticipantGroups.push_back("Non existing participantGroup");
  // Not both participantGroups and participants
  request.mPolymorphicPseudonyms = {};
  // Existing columnGroup
  request.mColumnGroups.push_back("Non existing columnGroup");
  // Existing column
  request.mColumns.push_back("Non existing column");


  try {
    // Act
    backend->checkTicketRequest(request);
    FAIL() << "This should not have run without exceptions.";
  }
  catch (const Error& e) {
    // Assert
    std::string expectedMessage = "Unknown participantgroup specified: \"Non existing participantGroup\"\nUnknown columngroup specified: \"Non existing columnGroup\"\nUnknown column specified: \"Non existing column\"";
    EXPECT_EQ(e.what(), expectedMessage);
  }
}

TEST_F(AccessManagerBackendTest, checkParticipantGroupAccess_happy) {

  std::vector<std::string> modes{"access", "enumerate"};
  Timestamp timestamp;
  backend->checkParticipantGroupAccess({constants.pg1}, constants.userGroup1, modes, timestamp);
  // No thrown exceptions means correct behaviour.
}

TEST_F(AccessManagerBackendTest, checkParticipantGroupAccess_no_access) {
  std::vector<std::string> modes{"access", "enumerate"};
  Timestamp timestamp;
  try {
    backend->checkParticipantGroupAccess({constants.pg2}, constants.userGroup1, modes, timestamp);
    FAIL() << "This should not have run without exceptions.";
  }
  catch (const Error& e) {
    std::string expectedMessage = "Access denied to \"" + constants.userGroup1 + "\" for mode \"" + modes[0] + "\" to participant-group \"" + constants.pg2 + "\"\nAccess denied to \"" + constants.userGroup1 + "\" for mode \"" + modes[1] + "\" to participant-group \"" + constants.pg2 + "\"";
    EXPECT_EQ(e.what(), expectedMessage);
  }
}

TEST_F(AccessManagerBackendTest, fillParticipantgroupMap_happy) {
  // Two polymorph pseudonyms without known participantgroups. Used to test the offset in IndexList
  std::vector<AccessManager::Backend::pp_t> prePPs{{constants.dummyPP, true}, {constants.dummyPP, true}};
  const std::vector<std::string> participantgroups{constants.pg1, constants.pg2};
  std::unordered_map<std::string, IndexList> actualParticipantGroupMap{};

  // Act
  backend->fillParticipantGroupMap(participantgroups, prePPs, actualParticipantGroupMap);

  // Assert
  EXPECT_EQ(actualParticipantGroupMap.size(), 2U); // The two participantGroups
  EXPECT_EQ(prePPs.size(), 4U); // The two pps defined in this tests, plus the two pps in the participantGroups.
}

TEST_F(AccessManagerBackendTest, checkTicketForEncryptionKeyRequest_happy) {
  auto request = std::make_shared<EncryptionKeyRequest>();
  Ticket2 ticket{};
  ticket.mColumns.push_back(constants.w_col);
  ticket.mModes.push_back("write");
  KeyRequestEntry entry;
  entry.mKeyBlindMode = KeyBlindMode::BLIND_MODE_BLIND; // Needs ticket mode write
  entry.mMetadata.setTag(constants.w_col); // specified col should be in ticket columns.
  request->mEntries.push_back({entry});


  backend->checkTicketForEncryptionKeyRequest(request, ticket);
  // No thrown exceptions means correct behaviour.
}

TEST_F(AccessManagerBackendTest, handleColumnAccessRequest_happy) {
  auto request = ColumnAccessRequest();
  request.requireModes.push_back("read");
  auto actual = backend->handleColumnAccessRequest(request, constants.userGroup1);


  ColumnAccess expected{};
  expected.columnGroups[constants.r_cg1].modes.push_back("read");
  expected.columnGroups[constants.r_cg1].columns.mIndices = {0, 1, 2};
  expected.columnGroups[constants.r_cg2].modes.push_back("read");
  expected.columnGroups[constants.r_cg2].columns.mIndices = {1};
  expected.columns = {constants.double_col, constants.r_col1, constants.r_col2};

  EXPECT_EQ(actual.columns, expected.columns);
  EXPECT_EQ(actual.columnGroups, expected.columnGroups);

}

TEST_F(AccessManagerBackendTest, handleColumnAccessRequest_happy_include_implicit) {
  auto request = ColumnAccessRequest();
  request.includeImplicitlyGranted = true;
  request.requireModes.push_back("read");
  auto actual = backend->handleColumnAccessRequest(request, constants.userGroup1);


  ColumnAccess expected{};
  expected.columnGroups[constants.r_cg1].modes.push_back("read");
  expected.columnGroups[constants.r_cg1].modes.push_back("read-meta");
  expected.columnGroups[constants.r_cg1].columns.mIndices = {0, 1, 2};
  expected.columnGroups[constants.r_cg2].modes.push_back("read");
  expected.columnGroups[constants.r_cg2].modes.push_back("read-meta");
  expected.columnGroups[constants.r_cg2].columns.mIndices = {1};
  expected.columns = {constants.double_col, constants.r_col1, constants.r_col2};

  EXPECT_EQ(actual.columns, expected.columns);
  EXPECT_EQ(actual.columnGroups, expected.columnGroups);
}

TEST_F(AccessManagerBackendTest, assertColumnAccess_no_access) {
  ColumnAccessRequest request;

  auto result = backend->handleColumnAccessRequest(request, constants.userGroup2);

  EXPECT_EQ(result.columnGroups.size(), 0);
  EXPECT_EQ(result.columns.size(), 0);
}
TEST_F(AccessManagerBackendTest, assertParticipantAccess_happy) {
  auto now = Timestamp();
  backend->assertParticipantAccess(constants.userGroup1, constants.localPseudonym1, {"access", "enumerate"}, now);
}

TEST_F(AccessManagerBackendTest, assertParticipantAccess_happy_star_participant) {

  auto now = Timestamp();
  // Research Assessor has no access to the participantgroup localPseudonym1 is in, but does have access to "*". This should pass.
  backend->assertParticipantAccess("Research Assessor", constants.localPseudonym1, {"access", "enumerate"}, now);
}

TEST_F(AccessManagerBackendTest, assertParticipantAccess_no_access) {
  auto now = Timestamp();
  try {
    // Act
    backend->assertParticipantAccess(constants.userGroup1, constants.localPseudonym2, {"access", "enumerate"}, now);
    FAIL() << "This should not have run without exceptions.";
  }
  catch (const Error& e) {
    // Assert
    std::string expectedMessage = "Access denied to participant for mode \"access\"\nAccess denied to participant for mode \"enumerate\"";
    EXPECT_EQ(e.what(), expectedMessage);
  }
}

TEST_F(AccessManagerBackendTest, AMAquery_noFilter){
  auto request = AmaQuery();
  auto response = backend->performAMAQuery(request, "Access Administrator");

  EXPECT_EQ(response.mColumns.size(), 65U);
  EXPECT_EQ(response.mColumnGroups.size(), 18U);
  EXPECT_EQ(response.mColumnGroupAccessRules.size(), 42U);
  EXPECT_EQ(response.mParticipantGroups.size(), 2U);
  EXPECT_EQ(response.mParticipantGroupAccessRules.size(), 12U);
}

TEST_F(AccessManagerBackendTest, AMAquery_OneColumnGroup){
  auto request = AmaQuery();
  request.mColumnGroupFilter = constants.r_cg1;
  auto response = backend->performAMAQuery(request, "Access Administrator");

  EXPECT_EQ(response.mColumns.size(), 3U);
  EXPECT_EQ(response.mColumnGroups.size(), 1U);
  EXPECT_EQ(response.mColumnGroupAccessRules.size(), 1U);
  EXPECT_EQ(response.mParticipantGroups.size(), 2U);
  EXPECT_EQ(response.mParticipantGroupAccessRules.size(), 12U);
}

TEST_F(AccessManagerBackendTest, AMAquery_OneParticipantGroup){
  auto request = AmaQuery();
  request.mParticipantGroupFilter = constants.pg1;
  auto response = backend->performAMAQuery(request, "Access Administrator");

  EXPECT_EQ(response.mColumns.size(), 65U);
  EXPECT_EQ(response.mColumnGroups.size(), 18U);
  EXPECT_EQ(response.mColumnGroupAccessRules.size(), 42U);
  EXPECT_EQ(response.mParticipantGroups.size(), 1U);
  EXPECT_EQ(response.mParticipantGroupAccessRules.size(), 2U);
}

TEST_F(AccessManagerBackendTest, AMAquery_OneUserGroup){
  auto request = AmaQuery();
  request.mUserGroupFilter = constants.userGroup1;
  auto response = backend->performAMAQuery(request, "Access Administrator");

  EXPECT_EQ(response.mColumns.size(), 6U);
  EXPECT_EQ(response.mColumnGroups.size(), 5U);
  EXPECT_EQ(response.mColumnGroupAccessRules.size(), 5U);
  EXPECT_EQ(response.mParticipantGroups.size(), 1U);
  EXPECT_EQ(response.mParticipantGroupAccessRules.size(), 2U);
}

TEST_F(AccessManagerBackendTest, AMAquery_MultipleFilters){
  auto request = AmaQuery();
  request.mUserGroupFilter = constants.userGroup1;
  request.mParticipantGroupFilter = constants.pg1;
  request.mColumnFilter = constants.r_col1;
  auto response = backend->performAMAQuery(request, "Access Administrator");

  EXPECT_EQ(response.mColumns.size(), 1U);
  EXPECT_EQ(response.mColumnGroups.size(), 2U);
  EXPECT_EQ(response.mColumnGroupAccessRules.size(), 2U);
  EXPECT_EQ(response.mParticipantGroups.size(), 1U);
  EXPECT_EQ(response.mParticipantGroupAccessRules.size(), 2U);
}

TEST_F(AccessManagerBackendTest, AMAquery_NonExistingUserGroup){
  auto request = AmaQuery();
  request.mUserGroupFilter = "non-existing";
  auto response = backend->performAMAQuery(request, "Access Administrator");

  EXPECT_EQ(response.mColumns.size(), 0U);
  EXPECT_EQ(response.mColumnGroups.size(), 0U);
  EXPECT_EQ(response.mColumnGroupAccessRules.size(), 0U);
  EXPECT_EQ(response.mParticipantGroups.size(), 0U);
  EXPECT_EQ(response.mParticipantGroupAccessRules.size(), 0U);
}

TEST_F(AccessManagerBackendTest, AMAquery_PartialColumnFilter){
  auto request = AmaQuery();
  request.mColumnFilter = "star";
  auto response = backend->performAMAQuery(request, "Access Administrator");

  EXPECT_EQ(response.mColumns.size(), 0U);
  EXPECT_EQ(response.mColumnGroups.size(), 0U);
  EXPECT_EQ(response.mColumnGroupAccessRules.size(), 0U);
  EXPECT_EQ(response.mParticipantGroups.size(), 2U);
  EXPECT_EQ(response.mParticipantGroupAccessRules.size(), 12U);
}

TEST_F(AccessManagerBackendTest, AMAquery_ColumnOnlyInStarFilter){
  auto request = AmaQuery();
  request.mColumnFilter = constants.star_col;
  auto response = backend->performAMAQuery(request, "Access Administrator");

  EXPECT_EQ(response.mColumns.size(), 1U);
  EXPECT_EQ(response.mColumnGroups.size(), 1U);
  EXPECT_EQ(response.mColumnGroupAccessRules.size(), 0U);
  EXPECT_EQ(response.mParticipantGroups.size(), 2U);
  EXPECT_EQ(response.mParticipantGroupAccessRules.size(), 12U);
}


TEST_F(AccessManagerBackendTest, handleSetMetadataRequestNoAccess) {
  SetStructureMetadataRequest request{
      .subjectType = StructureMetadataType::Column,
  };
  EXPECT_THROW(backend->handleSetStructureMetadataRequestHead(request, constants.userGroup1), Error)
      << "only Data Administrator should be able to set metadata";
}

TEST_F(AccessManagerBackendTest, handleFindUserRequest_returns_all_groups_for_existing_user) {
  for (auto& user : constants.users) {
    auto response = backend->handleFindUserRequest(FindUserRequest(user.primaryId, { user.displayId }), "Authserver");
    EXPECT_NE(response.mUserGroups, std::nullopt);
    EXPECT_THAT(*response.mUserGroups, testing::SizeIs(user.userGroups.size()));
    for (auto& group : *response.mUserGroups) {
      EXPECT_THAT(user.userGroups, testing::Contains(group.mName));
    }
  }
}

TEST_F(AccessManagerBackendTest, handleFindUserRequest_adds_primary_id_if_not_yet_known) {
  auto internalIdAtStart = storage->findInternalUserId(constants.user1.primaryId);
  ASSERT_NE(internalIdAtStart, std::nullopt);
  storage->removeIdentifierForUser(constants.user1.primaryId);
  ASSERT_EQ(storage->findInternalUserId(constants.user1.primaryId), std::nullopt);
  backend->handleFindUserRequest(FindUserRequest{constants.user1.primaryId, {constants.user1.displayId}},  "Authserver");
  EXPECT_EQ(storage->findInternalUserId(constants.user1.primaryId), internalIdAtStart);
}

TEST_F(AccessManagerBackendTest, handleFindUserRequest_returns_nullopt_for_non_existing_user) {
  auto response = backend->handleFindUserRequest(FindUserRequest{constants.nonExistingUser, {}}, "Authserver");
  EXPECT_EQ(response.mUserGroups, std::nullopt);
}

TEST_F(AccessManagerBackendTest, handleFindUserRequest_throws_when_primary_id_does_not_match) {
  EXPECT_THROW(backend->handleFindUserRequest(FindUserRequest{constants.unusedPrimaryId, {constants.user1.displayId}}, "Authserver"), Error);
  EXPECT_THROW(backend->handleFindUserRequest(FindUserRequest{constants.user1.displayId, {}}, "Authserver"), Error);
}

}
