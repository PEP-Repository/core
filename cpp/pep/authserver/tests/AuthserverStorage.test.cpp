#include <gtest/gtest.h>

#include <pep/authserver/AuthserverStorage.hpp>

namespace pep {
class AuthserverStorageTest : public ::testing::Test {
protected:
  AuthserverStorage storage;

public:
  AuthserverStorageTest() : storage(":memory:") {}
};

TEST_F(AuthserverStorageTest, internalIdIsAscending) {
  int64_t previousId = 0;
  int64_t currentId;
  for(size_t i = 0; i < 10; i++) {
    currentId = storage.createUser("user" + std::to_string(i));
    EXPECT_GT(currentId, previousId);
    previousId = currentId;
  }
}

TEST_F(AuthserverStorageTest, createUserUidMustBeUnique) {
  storage.createUser("user");
  EXPECT_ANY_THROW(storage.createUser("user"));
}

TEST_F(AuthserverStorageTest, findInternalId) {
  int64_t originalId = storage.createUser("user");
  storage.createUser("anotherUser");
  EXPECT_EQ(storage.findInternalId("user"), originalId);
  EXPECT_EQ(storage.findInternalId("NotExisting"), std::nullopt);
}

TEST_F(AuthserverStorageTest, multipleIdentifiers) {
  int64_t originalId = storage.createUser("user");
  storage.createUser("anotherUser");
  storage.addIdentifierForUser(originalId, "firstAlternativeName");
  storage.addIdentifierForUser(originalId, "secondAlternativeName");
  EXPECT_EQ(storage.findInternalId("firstAlternativeName"), originalId);
  EXPECT_EQ(storage.findInternalId("secondAlternativeName"), originalId);
  storage.removeIdentifierForUser(originalId, "secondAlternativeName");
  EXPECT_EQ(storage.findInternalId("firstAlternativeName"), originalId);
  EXPECT_EQ(storage.findInternalId("secondAlternativeName"), std::nullopt);

  storage.removeUser("user");
  EXPECT_EQ(storage.findInternalId("user"), std::nullopt);
  EXPECT_EQ(storage.findInternalId("firstAlternativeName"), std::nullopt);
}

TEST_F(AuthserverStorageTest, cannotRemoveLastIdentifier) {
  int64_t originalId = storage.createUser("user");
  storage.addIdentifierForUser(originalId, "firstAlternativeName");
  storage.addIdentifierForUser(originalId, "secondAlternativeName");
  storage.removeIdentifierForUser(originalId, "firstAlternativeName");
  storage.removeIdentifierForUser(originalId, "user");
  EXPECT_ANY_THROW(storage.removeIdentifierForUser(originalId, "secondAlternativeName"));
}

TEST_F(AuthserverStorageTest, cannotRemoveUidStillInGroups) {
  int64_t originalId = storage.createUser("user");
  storage.createGroup("group1", {});
  storage.addUserToGroup(originalId, "group1");
  EXPECT_ANY_THROW(storage.removeUser(originalId));
  storage.removeUserFromGroup(originalId, "group1");
  EXPECT_NO_THROW(storage.removeUser(originalId));
}

}