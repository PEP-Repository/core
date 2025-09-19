#include <pep/keyserver/tokenblocking/BlocklistEntry.hpp>
#include <pep/keyserver/tokenblocking/SqliteBlocklist.hpp>
#include <pep/keyserver/tokenblocking/TokenIdentifier.hpp>

#include <array>
#include <chrono>
#include <filesystem>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <pep/utils/Filesystem.hpp>
#include <span>
#include <unordered_set>

namespace {

/// Test suite for verifying concrete implementations of the Blocklist interface
template <typename T>
class TokenBlocking_Blocklist_Implementations;

/// Test suite for functions built on the TokenIdentifier struct
class TokenBlocking_TokenIdentifier;

/// Test suite for functions built on the BlocklistEntry struct
class TokenBlocking_BlocklistEntry;

/// Test suite for behaviors that are specific to the SqliteBlockList implementation
class TokenBlocking_SqliteBlocklist;

/// Test suite for functions on the Blocklist interface that are implementation agnostic
class TokenBlocking_Blocklist_Interface;

using namespace pep::tokenBlocking;
using std::chrono::days;
using ::testing::IsEmpty;

/// Overrides all virtual methods in Blocklist and makes them throw an exception.
class FakeBlockList : public Blocklist {
public:
  size_t size() const override { throw std::runtime_error{"not implemented"}; }

  std::vector<Entry> allEntries() const override { throw std::runtime_error{"not implemented"}; }

  std::vector<Entry> allEntriesMatching(const TokenIdentifier&) const override {
    throw std::runtime_error{"not implemented"};
  }

  std::optional<Entry> entryById(int64_t) const override { throw std::runtime_error{"not implemented"}; }

  int64_t add(const TokenIdentifier&, const Entry::Metadata&) override { throw std::runtime_error{"not implemented"}; }

  std::optional<Entry> removeById(int64_t) override { throw std::runtime_error{"not implemented"}; }
};

/// Some arbitrary point in time to be used as a reference point in tests.
constexpr auto REFERENCE_TIMESTAMP = pep::Timestamp{1577836800};

constexpr pep::Timestamp operator+ (pep::Timestamp lhs, std::chrono::days duration) {
  return pep::Timestamp{lhs.getTime() + duration_cast<std::chrono::milliseconds>(duration).count()};
}

constexpr pep::Timestamp operator- (pep::Timestamp lhs, std::chrono::days duration) {
  return operator+ (lhs, -duration);
}

pep::filesystem::Temporary MakeUniqueTempDir() {
  namespace fs = pep::filesystem;
  fs::Temporary tmp{fs::temp_directory_path() / fs::RandomizedName("pepTest-tokenBlocking-%%%%-%%%%-%%%%")};
  fs::create_directory(tmp.path());
  return tmp;
}

template <std::size_t size>
std::span<const TokenIdentifier> ArbitraryTokens() {
  const static std::array<TokenIdentifier, 4> hardcodedTestTokens{
      {{"subject_A", "groupA", REFERENCE_TIMESTAMP},
       {"subject_C", "groupB", REFERENCE_TIMESTAMP + days{3}},
       {"subject_B", "groupA", REFERENCE_TIMESTAMP + days{1}},
       {"subject_C", "groupB", REFERENCE_TIMESTAMP + days{1}}}};

  // passing the size as a template param allows us to do a static_assert on it
  static_assert(size <= hardcodedTestTokens.size(), "expand the array if a test needs more tokens");
  return {hardcodedTestTokens.data(), size};
}

template <typename T>
class TokenBlocking_Blocklist_Implementations : public testing::Test {
public:
  std::unique_ptr<T> createEmptyBlocklist() = delete;
};

using BlocklistImplementations = ::testing::Types<SqliteBlocklist>; // Types that will be tested against the interface

// Provide a specialization of TokenBlocking_Blocklist::createEmptyBlocklist for each type in BlocklistImplementations
template <>
std::unique_ptr<SqliteBlocklist> TokenBlocking_Blocklist_Implementations<SqliteBlocklist>::createEmptyBlocklist() {
  return SqliteBlocklist::CreateWithMemoryStorage();
}

TYPED_TEST_SUITE(TokenBlocking_Blocklist_Implementations, BlocklistImplementations);


// A token (identifier) A supersedes another token (identifier) B when
// A was issued after B and is otherwise equivalent to B.
TEST(TokenBlocking_TokenIdentifier, Supersedes) {
  const auto referencePoint = REFERENCE_TIMESTAMP + days{250}; // arbitrary point in time
  const auto oldToken =
      TokenIdentifier{.subject = "the_subject", .userGroup = "the_user_group", .issueDateTime = referencePoint};
  const auto newToken = TokenIdentifier{
      .subject = "the_subject",
      .userGroup = "the_user_group",
      .issueDateTime = referencePoint + days{5}};

  EXPECT_TRUE(Supersedes(newToken, oldToken));
}

// A token supersedes itself by definition.
TEST(TokenBlocking_TokenIdentifier, Supersedes_Itself) {
  const auto arbitraryTimepoint = REFERENCE_TIMESTAMP + days{150};
  const auto token =
      TokenIdentifier{.subject = "the_subject", .userGroup = "the_user_group", .issueDateTime = arbitraryTimepoint};

  EXPECT_TRUE(Supersedes(token, token));
}

// A rule does not apply to a token that has a different subject
TEST(TokenBlocking_TokenIdentifier, Supersedes_DifferentSubject) {
  const auto referencePoint = REFERENCE_TIMESTAMP + days{300}; // arbitrary point in time
  const auto original =
      TokenIdentifier{.subject = "subject_A", .userGroup = "user_group_A", .issueDateTime = referencePoint};
  const auto tokenWithDifferentSubject =
      TokenIdentifier{.subject = "subject_B", .userGroup = "user_group_A", .issueDateTime = referencePoint + days{10}};

  EXPECT_FALSE(Supersedes(original, tokenWithDifferentSubject));
}

// A rule does not apply to a token that has a different group
TEST(TokenBlocking_TokenIdentifier, Supersedes_DifferentUserGroup) {
  const auto boundary = REFERENCE_TIMESTAMP + days{433}; // arbitrary point in time
  const auto original = TokenIdentifier{.subject = "subject_A", .userGroup = "user_group_A", .issueDateTime = boundary};
  const auto tokenWithDifferentUserGroup =
      TokenIdentifier{.subject = "subject_A", .userGroup = "user_group_B", .issueDateTime = boundary - days{200}};

  EXPECT_FALSE(Supersedes(original, tokenWithDifferentUserGroup));
}

// A rule does not apply to a token that was issued after the issuedBefore boundary
TEST(TokenBlocking_TokenIdentifier, Supersedes_DifferentIssueDateTime) {
  const auto boundary = REFERENCE_TIMESTAMP + days{50}; // arbitrary point in time
  const auto original = TokenIdentifier{.subject = "subject_A", .userGroup = "user_group_A", .issueDateTime = boundary};
  const auto tokenIssuedAtALaterPoint =
      TokenIdentifier{.subject = "subject_A", .userGroup = "user_group_A", .issueDateTime = boundary + days{1}};

  EXPECT_FALSE(Supersedes(original, tokenIssuedAtALaterPoint));
}

// A blocklist entry blocks a token if the entries target matches or supersedes the token.
TEST(TokenBlocking_BlocklistEntry, IsBlocking) {
  const auto blockDateTime = REFERENCE_TIMESTAMP + days{50};
  const auto entry = BlocklistEntry{
      .id = 24,
      .target{.subject = "subject", .userGroup = "group", .issueDateTime = blockDateTime},
      .metadata{.note = "blocked for tests", .issuer = "tester", .creationDateTime = blockDateTime + days{2}}};
  const auto exactMatch = entry.target;
  const auto issuedBeforeBlockDateTime = TokenIdentifier{
      .subject = entry.target.subject,
      .userGroup = entry.target.userGroup,
      .issueDateTime = blockDateTime - days{5}};

  EXPECT_TRUE(IsBlocking(entry, exactMatch));
  EXPECT_TRUE(IsBlocking(entry, issuedBeforeBlockDateTime));
}

// A blocklist entry does not block a token if the entries target does not match or supersede the token.
TEST(TokenBlocking_BlocklistEntry, IsBlocking_False) {
  const auto blockDateTime = REFERENCE_TIMESTAMP + days{50};
  const auto entry = BlocklistEntry{
      .id = 29,
      .target{.subject = "subject", .userGroup = "group", .issueDateTime = blockDateTime},
      .metadata{.note = "blocked for tests", .issuer = "tester", .creationDateTime = blockDateTime + days{2}}};
  const auto differentSubject =
      TokenIdentifier{.subject = "different", .userGroup = entry.target.userGroup, .issueDateTime = blockDateTime};
  const auto differentUserGroup =
      TokenIdentifier{.subject = entry.target.subject, .userGroup = "different", .issueDateTime = blockDateTime};
  const auto issuedAfterBlockDateTime = TokenIdentifier{
      .subject = entry.target.subject,
      .userGroup = entry.target.userGroup,
      .issueDateTime = blockDateTime + days{5}};

  EXPECT_FALSE(IsBlocking(entry, differentSubject));
  EXPECT_FALSE(IsBlocking(entry, differentUserGroup));
  EXPECT_FALSE(IsBlocking(entry, issuedAfterBlockDateTime));
}

// A SqliteBlocklist is empty by default.
TEST(TokenBlocking_SqliteBlocklist, EmptyByDefault) {
  const auto tmpDir = MakeUniqueTempDir();
  const auto sqliteFile = tmpDir.path() / "database.sqlite";

  const auto blocklist = SqliteBlocklist::CreateWithStorageLocation(sqliteFile);
  ASSERT_NE(blocklist, nullptr);
  EXPECT_EQ(blocklist->size(), 0);
}

// The testsuite createEmptyBlocklist() method returns an empty blocklist.
TYPED_TEST(TokenBlocking_Blocklist_Implementations, TestSuiteCreateEmpty) {
  EXPECT_EQ(this->createEmptyBlocklist()->size(), 0);
}

// An empty blocklist does not match any token.
TYPED_TEST(TokenBlocking_Blocklist_Implementations, BlocksNothingByDefault) {
  using TestSuite = TokenBlocking_Blocklist_Implementations<TypeParam>;
  const std::unique_ptr<Blocklist> blocklist = TestSuite::createEmptyBlocklist();

  for (const auto& token : ArbitraryTokens<3>()) { EXPECT_THAT(blocklist->allEntriesMatching(token), IsEmpty()); }
}

// The blocklist retains an entry for each token that is added to it.
TYPED_TEST(TokenBlocking_Blocklist_Implementations, AddingTokens) {
  using TestSuite = TokenBlocking_Blocklist_Implementations<TypeParam>;
  using EntryTuple = std::pair<TokenIdentifier, BlocklistEntry::Metadata>;
  const auto identifiersWithMetadata = std::array<EntryTuple, 4>{
      {{{"user1@project.net", "researcher", REFERENCE_TIMESTAMP + days{1}},
        {"admin1@pep.cs.ru.nl", "obsolete", REFERENCE_TIMESTAMP + days{8}}},
       {{"user2@project.net", "researcher", REFERENCE_TIMESTAMP + days{2}},
        {"admin2@pep.cs.ru.nl", "compromised", REFERENCE_TIMESTAMP + days{9}}},
       {{"user3@project.net", "uploader", REFERENCE_TIMESTAMP + days{3}},
        {"admin1@pep.cs.ru.nl", "assigned to wrong user", REFERENCE_TIMESTAMP + days{4}}},
       {{"user4@project.net", "uploader", REFERENCE_TIMESTAMP + days{4}},
        {"admin2@pep.cs.ru.nl", "obsolete", REFERENCE_TIMESTAMP + days{12}}}}};
  auto blocklist = TestSuite::createEmptyBlocklist();

  for (auto& e : identifiersWithMetadata) { blocklist->add(e.first, e.second); }

  const auto entries = blocklist->allEntries();
  EXPECT_TRUE(std::ranges::is_permutation(
      entries,
      identifiersWithMetadata,
      std::equal_to<EntryTuple>{},                                                    // compare elements as tuples
      [](const BlocklistEntry& lhs) { return EntryTuple{lhs.target, lhs.metadata}; }, // entries to tuples
      std::identity{}));                                                              // other range to tuples
  EXPECT_EQ(blocklist->size(), 4);
}

// Blocklist::add returns the id of the added entry
TYPED_TEST(TokenBlocking_Blocklist_Implementations, AddingReturnsId) {
  using TestSuite = TokenBlocking_Blocklist_Implementations<TypeParam>;
  const auto tokens = ArbitraryTokens<3>();
  const auto emptyMetadata = Blocklist::Entry::Metadata{};
  const std::unique_ptr<Blocklist> blocklist = TestSuite::createEmptyBlocklist();

  for (const auto& token : tokens) {
    const auto returnedId = blocklist->add(token, emptyMetadata);

    const auto match = blocklist->allEntriesMatching(token);
    ASSERT_EQ(match.size(), 1);
    EXPECT_EQ(returnedId, match.front().id);
  }
}

// Blocklist entries have a unique id
TYPED_TEST(TokenBlocking_Blocklist_Implementations, UniqueIds) {
  using TestSuite = TokenBlocking_Blocklist_Implementations<TypeParam>;
  constexpr auto numberOfAdds = 4;
  const auto addedTokens = ArbitraryTokens<numberOfAdds>();
  const auto emptyMetadata = Blocklist::Entry::Metadata{};
  const std::unique_ptr<Blocklist> blocklist = TestSuite::createEmptyBlocklist();

  std::unordered_set<int64_t> uniqueIds;
  for (const auto& t : addedTokens) { uniqueIds.insert(blocklist->add(t, emptyMetadata)); }

  EXPECT_EQ(uniqueIds.size(), numberOfAdds);
}

// Blocklist entries can be retrieved by their id.
TYPED_TEST(TokenBlocking_Blocklist_Implementations, RetrieveById) {
  using TestSuite = TokenBlocking_Blocklist_Implementations<TypeParam>;
  constexpr auto tokenIssueDateTime = pep::Timestamp{0} + days{200};
  constexpr auto tokenBlockDateTime = tokenIssueDateTime + days{10};
  const auto arbitraryNoise = ArbitraryTokens<2>();

  const std::unique_ptr<Blocklist> blocklist = TestSuite::createEmptyBlocklist();
  blocklist->add(arbitraryNoise.front(), {}); // rules out just returning the first entry
  const auto id = blocklist->add(
      {.subject = "example_user@somewhere.org", .userGroup = "Research Assessor", .issueDateTime = tokenIssueDateTime},
      {.note = "Token was sent to the wrong person.",
       .issuer = "some_admin@somewhere.org",
       .creationDateTime = tokenBlockDateTime});
  blocklist->add(arbitraryNoise.back(), {}); // rules out just returning the last entry

  const auto expectedResult = BlocklistEntry{
      id,
      {.subject = "example_user@somewhere.org", .userGroup = "Research Assessor", .issueDateTime = tokenIssueDateTime},
      {.note = "Token was sent to the wrong person.",
       .issuer = "some_admin@somewhere.org",
       .creationDateTime = tokenBlockDateTime}};
  EXPECT_EQ(blocklist->entryById(id), expectedResult);
}

// Blocklist::getEntryById returns std::nullopt if the requested entry does no exist.
TYPED_TEST(TokenBlocking_Blocklist_Implementations, RetrieveById_NoEntry) {
  using TestSuite = TokenBlocking_Blocklist_Implementations<TypeParam>;
  const std::unique_ptr<Blocklist> blocklist = TestSuite::createEmptyBlocklist();

  EXPECT_FALSE(blocklist->entryById(1).has_value());
  EXPECT_FALSE(blocklist->entryById(0).has_value()); // edge-case: zero is never used as an id.
}

// Entries in a blocklist can be removed via their entry ids.
TYPED_TEST(TokenBlocking_Blocklist_Implementations, RemovingTokens) {
  using TestSuite = TokenBlocking_Blocklist_Implementations<TypeParam>;
  const auto initiallyBlockedTokens = ArbitraryTokens<3>();
  const std::unique_ptr<Blocklist> blocklist = TestSuite::createEmptyBlocklist();
  for (const auto& t : initiallyBlockedTokens) {
    blocklist->add(t, {"data_administrator", "for testing", pep::Timestamp::Min() + days{200}});
  }

  for (const auto& removedToken : initiallyBlockedTokens) {
    const auto matchBeforeRemoval = blocklist->allEntriesMatching(removedToken);
    ASSERT_FALSE(matchBeforeRemoval.empty());

    blocklist->removeById(matchBeforeRemoval.front().id);

    const auto matchAfterRemoval = blocklist->allEntriesMatching(removedToken);
    EXPECT_TRUE(matchAfterRemoval.empty());
  }
}

// Blocklist::remove returns the removed entry on success.
TYPED_TEST(TokenBlocking_Blocklist_Implementations, RemoveSuccess) {
  using TestSuite = TokenBlocking_Blocklist_Implementations<TypeParam>;
  const auto addedToken = ArbitraryTokens<1>().front();
  const auto metadata = Blocklist::Entry::Metadata{"issuer", "reason", REFERENCE_TIMESTAMP};
  auto blocklist = TestSuite::createEmptyBlocklist();
  const auto id = blocklist->add(addedToken, metadata);

  const auto expectedResult = Blocklist::Entry{id, addedToken, metadata};
  EXPECT_EQ(blocklist->removeById(id), expectedResult);
}

// Blocklist::remove returns nothing iff the item did not exist.
TYPED_TEST(TokenBlocking_Blocklist_Implementations, RemoveNonExisting) {
  using TestSuite = TokenBlocking_Blocklist_Implementations<TypeParam>;
  auto blocklist = TestSuite::createEmptyBlocklist();

  EXPECT_EQ(blocklist->removeById(23), std::nullopt);
}

// SqliteBlocklist::CreateWithStorageLocation returns an object with persistent storage
TEST(TokenBlocking_SqliteBlocklist, CreateWithStorageLocation) {
  const auto tmpDir = MakeUniqueTempDir();
  const auto sqliteFile = tmpDir.path() / "database.sqlite";
  const auto addedToken = TokenIdentifier{
      .subject = "user_user",
      .userGroup = "group_group",
      .issueDateTime = REFERENCE_TIMESTAMP + days{33}};
  const auto addedMetadata = Blocklist::Entry::Metadata{
      .note = "note_note",
      .issuer = "issuer_issuer",
      .creationDateTime = REFERENCE_TIMESTAMP + days{52}};

  { // original instance scope
    const auto instanceA = SqliteBlocklist::CreateWithStorageLocation(sqliteFile);
    EXPECT_TRUE(instanceA->isPersistent());

    instanceA->add(addedToken, addedMetadata);
  }

  { // new instance scope
    const auto instanceB = SqliteBlocklist::CreateWithStorageLocation(sqliteFile);
    EXPECT_TRUE(instanceB->isPersistent());

    const auto matches = instanceB->allEntriesMatching(addedToken);
    EXPECT_EQ(matches.size(), 1);
    EXPECT_EQ(matches.front().target, addedToken);
    EXPECT_EQ(matches.front().metadata, addedMetadata);
  }
}

// SqliteBlocklist::CreateWithStorageLocation throws a std::runtime_error if called with a sqlite3 special value.
TEST(TokenBlocking_SqliteBlocklist, CreateWithStorageLocation_RejectSpecialValues) {
  EXPECT_THROW(SqliteBlocklist::CreateWithStorageLocation("file:relativeUri.db"), std::runtime_error);
  EXPECT_THROW(SqliteBlocklist::CreateWithStorageLocation(""), std::runtime_error);
  EXPECT_THROW(SqliteBlocklist::CreateWithStorageLocation(":memory:"), std::runtime_error);

  // In a previous implementation, any path starting with whitespace chars would be accepted
  EXPECT_THROW(SqliteBlocklist::CreateWithStorageLocation("   file:withLeadingWhitespace"), std::runtime_error);
}

// SqliteBlocklist::Create returns an object with non-persistent storage when "" or ":memory:" is passed as argument.
TEST(TokenBlocking_SqliteBlocklist, CreateWithMemoryStorage) {
  const auto token = ArbitraryTokens<1>().front();
  const auto metadata = Blocklist::Entry::Metadata{};

  const auto instanceA = SqliteBlocklist::CreateWithMemoryStorage();
  EXPECT_FALSE(instanceA->isPersistent());
  EXPECT_EQ(instanceA->size(), 0);

  instanceA->add(token, metadata);
  EXPECT_EQ(instanceA->size(), 1);                                  // change should be visible in this instance
  EXPECT_EQ(SqliteBlocklist::CreateWithMemoryStorage()->size(), 0); // but not in any other instance
}

/// A token is blocked by a blocklist if there is at least one matching entry.
TEST(TokenBlocking_Blocklist_Interface, IsBlocking) {
  class : public FakeBlockList {
  public:
    std::size_t numberOfMatches = 1;

    std::vector<Entry> allEntriesMatching(const pep::tokenBlocking::TokenIdentifier& identifier) const override {
      return std::vector<Entry>(
          numberOfMatches,
          {.id = 1,
           .target = identifier,
           .metadata = {.note = "none", .issuer = "admin", .creationDateTime = identifier.issueDateTime + days{2}}});
    }
  } blocklist;
  const auto tokenA = ArbitraryTokens<2>().front();
  const auto tokenB = ArbitraryTokens<2>().back();

  blocklist.numberOfMatches = 0;
  EXPECT_FALSE(IsBlocking(blocklist, tokenA));
  EXPECT_FALSE(IsBlocking(blocklist, tokenB));

  blocklist.numberOfMatches = 1;
  EXPECT_TRUE(IsBlocking(blocklist, tokenA));

  blocklist.numberOfMatches = 4;
  EXPECT_TRUE(IsBlocking(blocklist, tokenB));
}

}
