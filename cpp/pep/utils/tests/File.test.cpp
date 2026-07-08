#include <pep/utils/File.hpp>

#include <filesystem>
#include <gtest/gtest.h>
#include <pep/utils/Defer.hpp>
#include <pep/utils/Filesystem.hpp>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {

const std::string content("lorem ipsum dolor sit amet");

/// Creates a directory with a randomized name within the system's temp directory.
pep::filesystem::Temporary CreateTestDir() {
  const auto path = fs::temp_directory_path() / pep::filesystem::RandomizedName("pepTest-file-%%%%-%%%%-%%%%");
  fs::create_directory(path);
  // Canonicalize so the returned path matches what fs::current_path() reports for it: on macOS
  // the temp dir lives under /var/folders/... where /var is a symlink to /private/var, which
  // fs::current_path() resolves but temp_directory_path() does not.
  return pep::filesystem::Temporary{fs::canonical(path)};
}


TEST(File, ExtensionRegex) {
  ASSERT_TRUE(pep::IsValidFileExtension(".txt"));
  ASSERT_TRUE(pep::IsValidFileExtension(".h"));
  ASSERT_TRUE(pep::IsValidFileExtension(".md5"));
  ASSERT_TRUE(pep::IsValidFileExtension(".tar.gz"));

  ASSERT_FALSE(pep::IsValidFileExtension(""));
  ASSERT_FALSE(pep::IsValidFileExtension(".h?"));
  ASSERT_FALSE(pep::IsValidFileExtension("nodot"));
  ASSERT_FALSE(pep::IsValidFileExtension(".h whitespace"));
}

TEST(File, WriteAndReadFile) {
  const auto testDir = CreateTestDir();
  const std::string pathExistingFile = (testDir.path() / "existing-file.txt").string();

  ASSERT_NO_THROW(pep::WriteFile(pathExistingFile, content));
  ASSERT_NO_THROW(pep::WriteFile(pathExistingFile.c_str(), content));

  ASSERT_EQ(pep::ReadFile(pathExistingFile), content);
  ASSERT_EQ(pep::ReadFile(pathExistingFile.c_str()), content);
  ASSERT_EQ(pep::ReadFile(fs::path(pathExistingFile)), content);

  ASSERT_EQ(pep::ReadFileIfExists(pathExistingFile), content);
}

TEST(File, ReadUnexistingFile) {
  const auto testDir = CreateTestDir();
  const std::string pathUnexistingFile = (testDir.path() / "unexisting-file.txt").string();

  ASSERT_EQ(pep::ReadFileIfExists(pathUnexistingFile), std::nullopt);

  ASSERT_THROW(pep::ReadFile(pathUnexistingFile), std::runtime_error);
  ASSERT_THROW(pep::ReadFile(pathUnexistingFile.c_str()), std::runtime_error);
  ASSERT_THROW(pep::ReadFile(fs::path(pathUnexistingFile)), std::runtime_error);
}

TEST(File, AppendDirectoryNameSuffix) {
  // We assume AppendDirectoryNameSuffix removes a trailing slash and dot
  // We have fs::absolute here to make it work on Windows as well (becomes e.g. "C:\abc\def-pending")
  EXPECT_EQ(pep::AppendDirectoryNameSuffix("/abc/def", "-pending"), fs::absolute("/abc/def-pending"));
  EXPECT_EQ(pep::AppendDirectoryNameSuffix("/abc/def/", "-pending"), fs::absolute("/abc/def-pending"));
  EXPECT_EQ(pep::AppendDirectoryNameSuffix("/abc/def/.", "-pending"), fs::absolute("/abc/def-pending"));
  EXPECT_EQ(pep::AppendDirectoryNameSuffix("/abc/def/./", "-pending"), fs::absolute("/abc/def-pending"));
  EXPECT_EQ(pep::AppendDirectoryNameSuffix("/abc/def/..", "-pending"), fs::absolute("/abc-pending"));
  EXPECT_EQ(pep::AppendDirectoryNameSuffix("/abc/def/../", "-pending"), fs::absolute("/abc-pending"));

  const auto testDir = CreateTestDir();
  const auto testPath = testDir.path();
  const auto oldWorkingDir = fs::current_path();
  PEP_DEFER(fs::current_path(oldWorkingDir));
  fs::current_path(testPath);
  const auto testPathSuffixed = testPath.parent_path() / (testPath.filename() += "-pending");
  EXPECT_EQ(pep::AppendDirectoryNameSuffix(".", "-pending"), testPathSuffixed);
  EXPECT_EQ(pep::AppendDirectoryNameSuffix("./", "-pending"), testPathSuffixed);
  const auto testPathSubdir = testPath / "subdir";
  fs::create_directory(testPathSubdir);
  fs::current_path(testPathSubdir);
  EXPECT_EQ(pep::AppendDirectoryNameSuffix("..", "-pending"), testPathSuffixed);

  EXPECT_THROW((void) pep::AppendDirectoryNameSuffix("/", "-pending"), std::runtime_error);
  EXPECT_THROW((void) pep::AppendDirectoryNameSuffix("/.", "-pending"), std::runtime_error);
  EXPECT_THROW((void) pep::AppendDirectoryNameSuffix("/./", "-pending"), std::runtime_error);
  EXPECT_THROW((void) pep::AppendDirectoryNameSuffix("", "-pending"), std::runtime_error);
}

}
