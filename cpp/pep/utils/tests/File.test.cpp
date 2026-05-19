#include <pep/utils/File.hpp>

#include <filesystem>
#include <gtest/gtest.h>
#include <pep/utils/Filesystem.hpp>
#include <stdexcept>

using namespace std::literals;

namespace {

const std::string content("lorem ipsum dolor sit amet");

/// Creates a directory with a randomized name within the system's temp directory.
pep::filesystem::Temporary CreateTestDir() {
  pep::filesystem::Temporary temp{std::filesystem::temp_directory_path()
                                  / pep::filesystem::RandomizedName("pepTest-file-%%%%-%%%%-%%%%")};
  std::filesystem::create_directory(temp.path());
  return temp;
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
  EXPECT_EQ(pep::GetParentDirectory("abc"), ".");
  EXPECT_EQ(pep::GetParentDirectory("abc/def/ghi/.."), "abc");
  EXPECT_EQ(pep::GetParentDirectory("abc/def/.."), ".");
  EXPECT_EQ(pep::GetParentDirectory("abc/def/../.."), "..");
  EXPECT_EQ(pep::GetParentDirectory("."), "..");
  EXPECT_EQ(pep::GetParentDirectory(".."), "../..");
  EXPECT_THROW(pep::GetParentDirectory(""), std::invalid_argument);
  EXPECT_THROW(pep::GetParentDirectory("/"), std::invalid_argument);

#ifdef _WIN32
  EXPECT_EQ(pep::StripTrailingSlash("C:abc/def"), "C:abc");
  EXPECT_EQ(pep::StripTrailingSlash("C:."), "C:..");
  EXPECT_EQ(pep::StripTrailingSlash("C:/abc/def/"), "C:/abc");
  EXPECT_THROW(pep::StripTrailingSlash("C:/"), std::invalid_argument);
#endif
}

}
