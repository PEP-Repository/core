#include <gtest/gtest.h>
#include <pep/utils/File.hpp>
#include <pep/structure/StructureSerializers.hpp>
#include <pep/accessmanager/tests/TestSuiteGlobalConfiguration.hpp>

#include <pep/accessmanager/Storage.hpp>

#include <filesystem>
namespace pep {

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
    globalConf = std::make_shared<GlobalConfiguration>(Serialization::FromJsonString<GlobalConfiguration>(TEST_SUITE_GLOBAL_CONFIGURATION));
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
  auto expected = std::vector<std::string>{"column-group-accessrule", "column-group-columns", "column-groups", "columns", "group-accessrule", "participant-group-participants", "participant-group-participants-v2", "participant-groups", "select-start-pseud", "select-start-pseud-v2"};
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
  Timestamp timestamp{};
  auto actual = storage->getColumns(timestamp);

  size_t expected{58};
  ASSERT_EQ(actual.size(), expected);
}
TEST_F(AccessManagerStorageTest, getColumns_deleted_column) {
  storage->removeColumn("DeviceHistory");
  Timestamp timestamp{};
  auto actual = storage->getColumns(timestamp);
  size_t expected{57};
  ASSERT_EQ(actual.size(), expected);
}
TEST_F(AccessManagerStorageTest, getColumnGroups_happy) {
  Timestamp timestamp{};
  auto actual = storage->getColumnGroups(timestamp);

  size_t expected{12};
  ASSERT_EQ(actual.size(), expected);
}

TEST_F(AccessManagerStorageTest, getColumnGroups_deleted_columnGroup) {
  storage->removeColumnGroup("Device", true);
  Timestamp timestamp{};
  auto actual = storage->getColumnGroups(timestamp);

  size_t expected{11};
  ASSERT_EQ(actual.size(), expected);
}
TEST_F(AccessManagerStorageTest, getColumnGroupColumns_happy) {
  Timestamp timestamp{};
  auto actual_beta = storage->getColumnGroupColumns(timestamp);

  size_t expected{133};
  ASSERT_EQ(actual_beta.size(), expected);
}

TEST_F(AccessManagerStorageTest, getColumnGroupColumns_deleted_columnGroup) {
  storage->removeColumnGroup("Device", true);
  Timestamp timestamp{};
  auto actual_beta = storage->getColumnGroupColumns(timestamp);

  size_t expected{131};
  ASSERT_EQ(actual_beta.size(), expected);
}

TEST_F(AccessManagerStorageTest, getColumnGroupColumns_deleted_column) {
  Timestamp timestampBefore{};
  auto actualBefore = storage->getColumnGroupColumns(timestampBefore);

  storage->removeColumn("DeviceHistory"); // removed from "Device" ColumnGroup and "*" ColumnGroup, 2 removals

  Timestamp timestampAfter{};
  auto actualAfter = storage->getColumnGroupColumns(timestampAfter);

  ASSERT_EQ(actualBefore.size() - actualAfter.size(), 2);
}

}
