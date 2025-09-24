#include <pep/utils/Filesystem.hpp>

#include <algorithm>
#include <fstream>
#include <gtest/gtest.h>
#include <ranges>
#include <string_view>

namespace {

// To clean up disk resources in case the pep::filesystem::Temporary objects under test fail to do so.
struct TestResource final {
  ~TestResource() {
    remove_all(path);
  }
  std::filesystem::path path;
};

/// Creates an empty file at \p p.
void touch(const std::filesystem::path& p) {
  std::ofstream{p}; // NOLINT(bugprone-unused-raii): We just want to invoke the constructor and destructor here.
  assert(exists(p));
}

class FilesystemTemporary_AnyPath : public testing::TestWithParam<std::filesystem::path> {};
INSTANTIATE_TEST_SUITE_P(FilesystemTemporary_AnyPath_Instance,
                         FilesystemTemporary_AnyPath,
                         testing::Values("/absolute/path/to/file",
                                         "/absolute/path/to/dir/",
                                         "../relative/file",
                                         "relative/dir/",
                                         "" // empty path
                                         ));


// If a pattern with a single % block is passed to RandomizedName
// it will return a string where that block is replaced with randomized characters.
TEST(FilesystemRandomizedName, PreAndPostFix) {
  const auto pattern = std::string_view{"Randomize here -->%%%%%%%%<--"};
  const auto result = pep::filesystem::RandomizedName(pattern);

  ASSERT_TRUE(result.starts_with("Randomize here -->"));
  EXPECT_TRUE(result.ends_with("<--"));
  ASSERT_EQ(pattern.size(), result.size());

  const auto replacedChars = result.substr(result.find('>') + 1, 8); // safe because of the previous ASSERT checks
  // We only test if the character were replaced and trust that replacement chars are selected randomly.
  EXPECT_TRUE(std::ranges::all_of(replacedChars, [](char c) { return isdigit(c) || islower(c); }));
}

// If a pattern with multiple % blocks is passed to RandomizedName
// it will return a string where all of these blocks are replaced with randomized characters.
TEST(FilesystemRandomizedName, Segmented) {
  const auto isDigitOrLowercaseAlpha = [](char c) { return isdigit(c) || islower(c); };
  const auto pattern = "%%%%-%%%%-%%%%-%%%%";
  const auto result = pep::filesystem::RandomizedName(pattern);

  // We only test if the character were replaced and trust that replacement chars are selected randomly.
  EXPECT_TRUE(std::ranges::all_of(result.substr(0, 4), isDigitOrLowercaseAlpha));
  EXPECT_EQ(result[4], '-');
  EXPECT_TRUE(std::ranges::all_of(result.substr(5, 4), isDigitOrLowercaseAlpha));
  EXPECT_EQ(result[9], '-');
  EXPECT_TRUE(std::ranges::all_of(result.substr(10, 4), isDigitOrLowercaseAlpha));
  EXPECT_EQ(result[14], '-');
  EXPECT_TRUE(std::ranges::all_of(result.substr(15, 4), isDigitOrLowercaseAlpha));
}

// Passing an empty string to RandomizedName results in an empty string
TEST(FilesystemRandomizedName, EmptyPattern) {
  EXPECT_EQ(pep::filesystem::RandomizedName(""), "");
}

// A default constructed Temporary is empty
TEST(FilesystemTemporary, DefaultConstructor) {
  pep::filesystem::Temporary defaultConstructedTemporary;

  EXPECT_TRUE(defaultConstructedTemporary.path().empty());
}

// A Temporary constructed with a path contains that path.
TEST_P(FilesystemTemporary_AnyPath, ValueConstructor) {
  const auto constructorArgument = GetParam();
  pep::filesystem::Temporary valueConstructedTemporary{constructorArgument};

  EXPECT_EQ(valueConstructedTemporary.path(), constructorArgument);
}

// A Temporary is empty if its assigned path is empty
TEST(FilesystemTemporary, IsEmpty) {
  const auto nonEmptyTemporary = pep::filesystem::Temporary{"/non/empty/path"};
  EXPECT_FALSE(nonEmptyTemporary.empty());

  const auto defaultConstructedEmptyTemporary = pep::filesystem::Temporary{""};
  EXPECT_TRUE(defaultConstructedEmptyTemporary.empty());

  const auto valueConstructedEmptyTemporary = pep::filesystem::Temporary{};
  EXPECT_TRUE(valueConstructedEmptyTemporary.empty());
}

// Calling release on a Temporary leaves it empty
TEST_P(FilesystemTemporary_AnyPath, EmptyAfterRelease) {
  const auto initialPath = GetParam();
  pep::filesystem::Temporary valueInitialized{initialPath};

  valueInitialized.release();

  EXPECT_TRUE(valueInitialized.empty());
}

// Calling release on a Temporary returns the current path
TEST_P(FilesystemTemporary_AnyPath, ReleaseReturnsCurrentPath) {
  const auto initialPath = GetParam();
  pep::filesystem::Temporary valueInitialized{initialPath};

  EXPECT_EQ(valueInitialized.release(), initialPath);
}

// Calling release on a Temporary avoids that the file is deleted together with the Temporary.
TEST(FilesystemTemporary, ReleaseAvoidsDeletion) {
  const TestResource file{std::filesystem::temp_directory_path() / pep::filesystem::RandomizedName("peptest-%%%%%%%%")};
  touch(file.path);

  { // scope of temporaryFile
    pep::filesystem::Temporary temporaryFile{file.path};

    temporaryFile.release();
  }
  EXPECT_TRUE(exists(file.path));
}

// Re-assigning a Temporary triggers deletion of the disk resource that was bound to it.
TEST_P(FilesystemTemporary_AnyPath, ReassignmentTriggersDelete) {
  const auto reassignedPath = GetParam();
  const TestResource file{std::filesystem::temp_directory_path() / pep::filesystem::RandomizedName("peptest-%%%%%%%%")};
  touch(file.path);
  pep::filesystem::Temporary temporaryFile{file.path};

  temporaryFile = pep::filesystem::Temporary{reassignedPath};

  EXPECT_FALSE(exists(file.path));
}

// Re-assigning a Temporary to the same value does NOT trigger deletion of the disk resource.
TEST(FilesystemTemporary, ReassignToSameValue) {
  const TestResource file{std::filesystem::temp_directory_path() / pep::filesystem::RandomizedName("peptest-%%%%%%%%")};
  touch(file.path);
  pep::filesystem::Temporary temporaryFile{file.path};

  temporaryFile = pep::filesystem::Temporary{file.path};

  EXPECT_TRUE(exists(file.path));
}

TEST(FilesystemTemporary, DeletesResourceOnDestruction) {
  const TestResource testDir{std::filesystem::temp_directory_path()
                             / pep::filesystem::RandomizedName("peptest-%%%%%%%%")};

  const auto rootPath = testDir.path;
  const auto subdirPath = rootPath / "subdir";
  const auto fileAPath = rootPath / "subdir" / "fileA";
  const auto fileBPath = rootPath / "subdir" / "fileB";
  create_directories(subdirPath);
  touch(fileAPath);
  touch(fileBPath);

  { // root scope
    pep::filesystem::Temporary root{rootPath};
    { // subdir + fileB scope
      pep::filesystem::Temporary subdir{subdirPath};
      { // fileA scope
        pep::filesystem::Temporary fileA{fileAPath};
      }
      EXPECT_FALSE(exists(fileAPath)); // case: destroying a single file
      EXPECT_TRUE(exists(fileBPath));
    }
    EXPECT_FALSE(exists(subdirPath)); // case: destroying a non-empty directory (still contained fileB)
  }
  EXPECT_FALSE(exists(rootPath)); // case: destroying an empty directory
}

// Destroying a Temporary that is bound to a non-existing resource is not an error.
TEST(FilesystemTemporary, DestructWithNonExistingResource) {
  EXPECT_NO_FATAL_FAILURE(pep::filesystem::Temporary{""});

  const auto nonExisting = std::filesystem::temp_directory_path() / pep::filesystem::RandomizedName("peptest-%%%%%%%%");
  EXPECT_NO_FATAL_FAILURE(pep::filesystem::Temporary{nonExisting});
}

}
