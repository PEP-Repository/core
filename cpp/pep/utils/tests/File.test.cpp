#include <pep/utils/File.hpp>

#include <filesystem>
#include <gtest/gtest.h>
#include <pep/utils/Defer.hpp>
#include <pep/utils/Filesystem.hpp>
#include <stdexcept>

using namespace std::literals;

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
  EXPECT_TRUE(pep::IsValidFileExtension(".txt"));
  EXPECT_TRUE(pep::IsValidFileExtension(".h"));
  EXPECT_TRUE(pep::IsValidFileExtension(".md5"));
  EXPECT_TRUE(pep::IsValidFileExtension(".tar.gz"));

  EXPECT_FALSE(pep::IsValidFileExtension(""));
  EXPECT_FALSE(pep::IsValidFileExtension(".h?"));
  EXPECT_FALSE(pep::IsValidFileExtension("nodot"));
  EXPECT_FALSE(pep::IsValidFileExtension(".h whitespace"));
}

void TestGenericFileName(bool (*testFileName)(std::string_view)) {
  EXPECT_TRUE(testFileName("abc"));
  EXPECT_TRUE(testFileName("abc.txt"));

  EXPECT_FALSE(testFileName(""));
  EXPECT_FALSE(testFileName("."));
  EXPECT_FALSE(testFileName(".."));
  EXPECT_FALSE(testFileName("../abc"));
  EXPECT_FALSE(testFileName("abc/def"));
  EXPECT_FALSE(testFileName("/abc"));
  EXPECT_FALSE(testFileName("abc\0def"sv));
}

TEST(File, IsValidUnixFileName) {
  TestGenericFileName(pep::IsValidUnixFileName);
}

TEST(File, IsValidWindowsFileName) {
  TestGenericFileName(pep::IsValidWindowsFileName);

  EXPECT_FALSE(pep::IsValidWindowsFileName("abc\\def"));
  EXPECT_FALSE(pep::IsValidWindowsFileName("\\abc"));
  EXPECT_FALSE(pep::IsValidWindowsFileName("C:abc"));

  EXPECT_FALSE(pep::IsValidWindowsFileName("NUL"));
  EXPECT_FALSE(pep::IsValidWindowsFileName("nuL"));
  EXPECT_FALSE(pep::IsValidWindowsFileName("NUL.tar.gz"));
  EXPECT_FALSE(pep::IsValidWindowsFileName("abc."));
  EXPECT_FALSE(pep::IsValidWindowsFileName("abc "));
  EXPECT_FALSE(pep::IsValidWindowsFileName("ab*c"));
  EXPECT_FALSE(pep::IsValidWindowsFileName("ab\tc"));
  EXPECT_FALSE(pep::IsValidWindowsFileName("abc:"));
}

TEST(File, WriteAndReadFile) {
  const auto testDir = CreateTestDir();
  const auto pathExistingFile = testDir.path() / "existing-file.txt";

  EXPECT_NO_THROW(pep::WriteFile(pathExistingFile, content));

  EXPECT_EQ(pep::ReadFile(pathExistingFile), content);

  EXPECT_EQ(pep::ReadFileIfExists(pathExistingFile), content);
}

TEST(File, ReadUnexistingFile) {
  const auto testDir = CreateTestDir();
  const auto pathUnexistingFile = testDir.path() / "unexisting-file.txt";

  EXPECT_EQ(pep::ReadFileIfExists(pathUnexistingFile), std::nullopt);

  EXPECT_THROW(pep::ReadFile(pathUnexistingFile), std::runtime_error);
}

TEST(File, IsLexicallyRelativeChildPath) {
  EXPECT_TRUE(pep::IsLexicallyRelativeChildPath("abc"));
  EXPECT_TRUE(pep::IsLexicallyRelativeChildPath("abc/def"));
  EXPECT_TRUE(pep::IsLexicallyRelativeChildPath("abc/def/"));
  EXPECT_TRUE(pep::IsLexicallyRelativeChildPath("./abc"));
  EXPECT_TRUE(pep::IsLexicallyRelativeChildPath("abc/./def"));
  EXPECT_TRUE(pep::IsLexicallyRelativeChildPath("abc/../def"));
  EXPECT_TRUE(pep::IsLexicallyRelativeChildPath("abc/.."));

  EXPECT_FALSE(pep::IsLexicallyRelativeChildPath(".."));
  EXPECT_FALSE(pep::IsLexicallyRelativeChildPath("abc/../.."));
  EXPECT_FALSE(pep::IsLexicallyRelativeChildPath("../abc"));
  EXPECT_FALSE(pep::IsLexicallyRelativeChildPath("./../abc"));
  EXPECT_FALSE(pep::IsLexicallyRelativeChildPath("../../abc"));
  EXPECT_FALSE(pep::IsLexicallyRelativeChildPath("abc/../../def"));

  EXPECT_FALSE(pep::IsLexicallyRelativeChildPath("abc/", false));
  EXPECT_FALSE(pep::IsLexicallyRelativeChildPath(".", false));
  EXPECT_FALSE(pep::IsLexicallyRelativeChildPath("abc/..", false));

  EXPECT_FALSE(pep::IsLexicallyRelativeChildPath(""));

#ifdef _WIN32
  EXPECT_FALSE(pep::IsLexicallyRelativeChildPath("C:"));
  EXPECT_FALSE(pep::IsLexicallyRelativeChildPath("C:/"));
  EXPECT_FALSE(pep::IsLexicallyRelativeChildPath("C:abc"));
  EXPECT_FALSE(pep::IsLexicallyRelativeChildPath("//myserver/abc")); // UNC name
#endif
}

TEST(File, StripTrailingSlash) {
  EXPECT_EQ(pep::StripTrailingSlash("abc/def/"), "abc/def");
  EXPECT_EQ(pep::StripTrailingSlash("/abc/def/"), "/abc/def");
  EXPECT_EQ(pep::StripTrailingSlash("abc/def"), "abc/def");
  EXPECT_EQ(pep::StripTrailingSlash("/"), "/");

#ifdef _WIN32
  EXPECT_EQ(pep::StripTrailingSlash("C:/"), "C:/");
#endif
}

TEST(File, GetParentDirectory) {
  EXPECT_EQ(pep::GetParentDirectory("abc/def"), "abc");
  EXPECT_EQ(pep::GetParentDirectory("abc/def/"), "abc");
  EXPECT_EQ(pep::GetParentDirectory("abc/def/."), "abc");
  EXPECT_EQ(pep::GetParentDirectory("/abc/def"), fs::path{"/abc"}.make_preferred());
  EXPECT_EQ(pep::GetParentDirectory("/abc"), fs::path{"/"}.make_preferred());
  EXPECT_EQ(pep::GetParentDirectory("abc"), ".");
  EXPECT_EQ(pep::GetParentDirectory("abc/def/ghi/.."), "abc");
  EXPECT_EQ(pep::GetParentDirectory("abc/def/.."), ".");
  EXPECT_EQ(pep::GetParentDirectory("abc/def/../.."), "..");
  EXPECT_EQ(pep::GetParentDirectory("."), "..");
  EXPECT_EQ(pep::GetParentDirectory(".."), fs::path{".."} / fs::path{".."});
  EXPECT_THROW(pep::GetParentDirectory(""), std::invalid_argument);
  EXPECT_THROW(pep::GetParentDirectory("/"), std::invalid_argument);

#ifdef _WIN32
  EXPECT_EQ(pep::GetParentDirectory("C:abc/def"), "C:abc");
  EXPECT_EQ(pep::GetParentDirectory("C:abc"), "C:");
  EXPECT_EQ(pep::GetParentDirectory("C:."), "C:..");
  EXPECT_EQ(pep::GetParentDirectory(""), "..");
  EXPECT_EQ(pep::GetParentDirectory("C:"), "C:..");
  EXPECT_EQ(pep::GetParentDirectory("C:/abc/def/"), R"(C:\abc)");
  EXPECT_THROW(pep::GetParentDirectory("C:/"), std::invalid_argument);
#endif
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

  EXPECT_THROW((void) pep::AppendDirectoryNameSuffix("/", "-pending"), std::invalid_argument);
  EXPECT_THROW((void) pep::AppendDirectoryNameSuffix("/.", "-pending"), std::invalid_argument);
  EXPECT_THROW((void) pep::AppendDirectoryNameSuffix("/./", "-pending"), std::invalid_argument);
  EXPECT_THROW((void) pep::AppendDirectoryNameSuffix("", "-pending"), std::invalid_argument);
}

}
