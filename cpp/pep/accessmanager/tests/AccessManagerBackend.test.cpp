#include <gtest/gtest.h>
#include <pep/utils/File.hpp>
#include <pep/structure/StructureSerializers.hpp>
#include <pep/accessmanager/tests/TestSuiteGlobalConfiguration.hpp>

#include <pep/accessmanager/Storage.hpp>

#include <filesystem>
namespace pep {
/* This testsuite aims to test all interactions with the AccessManager::Backend that involve logic in the backend layer.
* For any pass through functionality, such as addParticipantToGroup(), see AccessManagerStorage.test.cpp
*
*/

class AccessManagerBackendTest : public ::testing::Test {
public:

  static std::shared_ptr<AccessManager::Backend> accessManagerBackend;
  static std::shared_ptr<AccessManager::Backend::Storage> storage; // Have a direct variable so we can check the storage state directly, without going through accessManagerBackend
  static std::shared_ptr<GlobalConfiguration> globalConf;
  struct Constants {
    const std::filesystem::path databasePath{"./testDB.sql"};

    const std::string userGroup1{"TestUserGroup"};
    const std::string userGroup2{"TestUserGroupWithoutAccess"};

    const std::string r_col1{"readColumn_1"};
    const std::string r_col2{"readColumn_2"};
    const std::string r_cg1{"readColumnGroup1"};
    const std::string r_cg2{"readColumnGroup2"};

    const std::string w_col{"writeColumn1"};
    const std::string w_cg{"writeColumnGroup"};

    const std::string pg1{"participantGroup_1"};
    const std::string pg2{"participantGroup_2"};

    const std::string rm_col{"readMetaColumn"};
    const std::string rm_cg{"readMetaColumnGroup"};

    const std::string wm_col{"writeMetaColumn"};
    const std::string wm_cg{"writeMetaColumnGroup"};

    const std::string star_col{"starColumn"}; // This column will not be in any columnGroup, except for "*".
    const std::string double_col{"doubleColumn"}; // This column is in both ReadColumnGroup1 and WriteColumnGroup, giving the user both access rights, through two paths.

    const LocalPseudonym localPseudonym1{LocalPseudonym::Random()};
    const LocalPseudonym localPseudonym2{LocalPseudonym::Random()};

    const PolymorphicPseudonym dummyPP{PolymorphicPseudonym::FromIdentifier(ElgamalPublicKey::Random(), "dummy")};
  };

  static Constants constants;

  static void SetUpTestSuite() {
    globalConf = std::make_shared<GlobalConfiguration>(Serialization::FromJsonString<GlobalConfiguration>(TEST_SUITE_GLOBAL_CONFIGURATION));
    std::filesystem::remove(constants.databasePath);
    storage = std::make_shared<AccessManager::Backend::Storage>(constants.databasePath, globalConf);
    accessManagerBackend = std::make_shared<AccessManager::Backend>(storage);
    populateDatabase();
  }

  static void TearDownTestSuite() {
    std::filesystem::remove(constants.databasePath);
  }


  // Create a basic administration with a few columngroups and participantgroups defined.
  static void populateDatabase() {
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

    // Column with both read and write access, but through two diufferent columngroups
    storage->createColumn(constants.double_col);
    storage->addColumnToGroup(constants.double_col, constants.r_cg1);
    storage->addColumnToGroup(constants.double_col, constants.w_cg);
  }
};

std::shared_ptr<AccessManager::Backend> AccessManagerBackendTest::accessManagerBackend;
AccessManagerBackendTest::Constants AccessManagerBackendTest::constants;
std::shared_ptr<AccessManager::Backend::Storage> AccessManagerBackendTest::storage;
std::shared_ptr<GlobalConfiguration> AccessManagerBackendTest::globalConf;

TEST_F(AccessManagerBackendTest, unfoldColumnGroupsAndAssertAccess_happy) {

  const std::vector<std::string> columngroups{constants.r_cg1};
  const std::vector<std::string> modes{"read"};
  Timestamp timestamp{};
  std::vector<std::string> columns;
  std::unordered_map<std::string, IndexList> columnGroupMap{};

  accessManagerBackend->unfoldColumnGroupsAndAssertAccess(constants.userGroup1, columngroups, modes, timestamp, columns, columnGroupMap);

  std::unordered_map<std::string, IndexList> expectedColumnGroupMap{{constants.r_cg1, IndexList({0, 1, 2})}};
  std::vector<std::string> expectedColumns = {constants.double_col, constants.r_col1, constants.r_col2};

  //checkMapEquality(columnGroupMap, expectedColumnGroupMap);
  ASSERT_EQ(columnGroupMap, expectedColumnGroupMap);

  // Sort calculated and expected values the same way to prevent failure due to ordering differences
  std::sort(columns.begin(), columns.end());
  std::sort(expectedColumns.begin(), expectedColumns.end());

  ASSERT_EQ(columns, expectedColumns);
}

TEST_F(AccessManagerBackendTest, unfoldColumnGroupsAndAssertAccess_column_access_through_multiple_column_groups_no_column_groups_in_request) {
  // The userGroup has read and write acces to the column, but through different columngroups. Access should be granted.
  const std::vector<std::string> columngroups{};
  const std::vector<std::string> modes{"read", "write"};
  Timestamp timestamp{};
  std::vector<std::string> columns{constants.double_col};
  std::unordered_map<std::string, IndexList> columnGroupMap{};
  accessManagerBackend->unfoldColumnGroupsAndAssertAccess(constants.userGroup1, columngroups, modes, timestamp, columns, columnGroupMap);

  std::unordered_map<std::string, IndexList> expectedColumnGroupMap{};
  std::vector<std::string> expectedColumns = {constants.double_col};

  ASSERT_EQ(columnGroupMap, expectedColumnGroupMap);
  ASSERT_EQ(columns, expectedColumns);
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
    accessManagerBackend->unfoldColumnGroupsAndAssertAccess(constants.userGroup1, columngroups, modes, timestamp, columns, columnGroupMap);

    FAIL() << "This should not have run without exceptions.";
  }
  catch (const Error& e) {
    // Assert
    std::string expectedMessage = "Access denied to \"TestUserGroup\" for mode \"read\" to column \"writeColumn1\"";
    ASSERT_EQ(e.what(), expectedMessage);
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

  accessManagerBackend->checkTicketRequest(request);
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
    accessManagerBackend->checkTicketRequest(request);

    FAIL() << "This should not have run without exceptions.";
  }
  catch (const Error& e) {
    // Assert
    std::string expectedMessage = "The ticket request contains participant group(s) as well as specific participant(s). This is not supported. Use either groups or specific participants.";
    ASSERT_EQ(e.what(), expectedMessage);
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
    accessManagerBackend->checkTicketRequest(request);
    FAIL() << "This should not have run without exceptions.";
  }
  catch (const Error& e) {
    // Assert
    std::string expectedMessage = "Unknown participantgroup specified: \"Non existing participantGroup\"\nUnknown columngroup specified: \"Non existing columnGroup\"\nUnknown column specified: \"Non existing column\"";
    ASSERT_EQ(e.what(), expectedMessage);
  }
}

TEST_F(AccessManagerBackendTest, checkParticipantGroupAccess_happy) {

  std::vector<std::string> modes{"access", "enumerate"};
  Timestamp timestamp;
  accessManagerBackend->checkParticipantGroupAccess({constants.pg1}, constants.userGroup1, modes, timestamp);
  // No thrown exceptions means correct behaviour.
}

TEST_F(AccessManagerBackendTest, checkParticipantGroupAccess_no_access) {
  std::vector<std::string> modes{"access", "enumerate"};
  Timestamp timestamp;
  try {
    accessManagerBackend->checkParticipantGroupAccess({constants.pg2}, constants.userGroup1, modes, timestamp);
    FAIL() << "This should not have run without exceptions.";
  }
  catch (const Error& e) {
    std::string expectedMessage = "Access denied to \"" + constants.userGroup1 + "\" for mode \"" + modes[0] + "\" to participant-group \"" + constants.pg2 + "\"\nAccess denied to \"" + constants.userGroup1 + "\" for mode \"" + modes[1] + "\" to participant-group \"" + constants.pg2 + "\"";
    ASSERT_EQ(e.what(), expectedMessage);
  }
}

TEST_F(AccessManagerBackendTest, fillParticipantgroupMap_happy) {
  // Two polymorph pseudonyms without known participantgroups. Used to test the offset in IndexList
  std::vector<AccessManager::Backend::pp_t> prePPs{{constants.dummyPP, true}, {constants.dummyPP, true}};
  const std::vector<std::string> participantgroups{constants.pg1, constants.pg2};
  std::unordered_map<std::string, IndexList> actualParticipantGroupMap{};

  // Act
  accessManagerBackend->fillParticipantGroupMap(participantgroups, prePPs, actualParticipantGroupMap);

  // Assert
  ASSERT_EQ(actualParticipantGroupMap.size(), 2U); // The two participantGroups
  ASSERT_EQ(prePPs.size(), 4U); // The two pps defined in this tests, plus the two pps in the participantGroups.
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


  accessManagerBackend->checkTicketForEncryptionKeyRequest(request, ticket);
  // No thrown exceptions means correct behaviour.
}

TEST_F(AccessManagerBackendTest, handleColumnAccessRequest_happy) {
  auto request = ColumnAccessRequest();
  request.requireModes.push_back("read");
  auto actual = accessManagerBackend->handleColumnAccessRequest(request, constants.userGroup1);


  ColumnAccess expected{};
  expected.columnGroups[constants.r_cg1].modes.push_back("read");
  expected.columnGroups[constants.r_cg1].columns.mIndices = {0, 1, 2};
  expected.columnGroups[constants.r_cg2].modes.push_back("read");
  expected.columnGroups[constants.r_cg2].columns.mIndices = {1};
  expected.columns = {constants.double_col, constants.r_col1, constants.r_col2};

  ASSERT_EQ(actual.columns, expected.columns);
  ASSERT_EQ(actual.columnGroups, expected.columnGroups);

}

TEST_F(AccessManagerBackendTest, handleColumnAccessRequest_happy_include_implicit) {
  auto request = ColumnAccessRequest();
  request.includeImplicitlyGranted = true;
  request.requireModes.push_back("read");
  auto actual = accessManagerBackend->handleColumnAccessRequest(request, constants.userGroup1);


  ColumnAccess expected{};
  expected.columnGroups[constants.r_cg1].modes.push_back("read");
  expected.columnGroups[constants.r_cg1].modes.push_back("read-meta");
  expected.columnGroups[constants.r_cg1].columns.mIndices = {0, 1, 2};
  expected.columnGroups[constants.r_cg2].modes.push_back("read");
  expected.columnGroups[constants.r_cg2].modes.push_back("read-meta");
  expected.columnGroups[constants.r_cg2].columns.mIndices = {1};
  expected.columns = {constants.double_col, constants.r_col1, constants.r_col2};

  ASSERT_EQ(actual.columns, expected.columns);
  ASSERT_EQ(actual.columnGroups, expected.columnGroups);
}

TEST_F(AccessManagerBackendTest, assertColumnAccess_no_access) {
  auto now = Timestamp();
  ColumnAccessRequest request;

  auto result = accessManagerBackend->handleColumnAccessRequest(request, constants.userGroup2);

  ASSERT_EQ(result.columnGroups.size(), 0);
  ASSERT_EQ(result.columns.size(), 0);
}
TEST_F(AccessManagerBackendTest, assertParticipantAccess_happy) {
  auto now = Timestamp();
  accessManagerBackend->assertParticipantAccess(constants.userGroup1, constants.localPseudonym1, {"access", "enumerate"}, now);
}

TEST_F(AccessManagerBackendTest, assertParticipantAccess_happy_star_participant) {

  auto now = Timestamp();
  // Research Assessor has no access to the participantgroup localPseudonym1 is in, but does have access to "*". This should pass.
  accessManagerBackend->assertParticipantAccess("Research Assessor", constants.localPseudonym1, {"access", "enumerate"}, now);
}

TEST_F(AccessManagerBackendTest, assertParticipantAccess_no_access) {
  auto now = Timestamp();
  try {
    // Act
    accessManagerBackend->assertParticipantAccess(constants.userGroup1, constants.localPseudonym2, {"access", "enumerate"}, now);
    FAIL() << "This should not have run without exceptions.";
  }
  catch (const Error& e) {
    // Assert
    std::string expectedMessage = "Access denied to participant for mode \"access\"\nAccess denied to participant for mode \"enumerate\"";
    ASSERT_EQ(e.what(), expectedMessage);
  }
}

}