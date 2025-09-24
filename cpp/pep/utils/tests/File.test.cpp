#include <pep/utils/File.hpp>

#include <filesystem>
#include <gtest/gtest.h>
#include <pep/utils/Filesystem.hpp>
#include <stdexcept>

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
  ASSERT_EQ(pep::ReadFile(std::filesystem::path(pathExistingFile)), content);

  ASSERT_EQ(pep::ReadFileIfExists(pathExistingFile), content);
}

TEST(File, ReadUnexistingFile) {
  const auto testDir = CreateTestDir();
  const std::string pathUnexistingFile = (testDir.path() / "unexisting-file.txt").string();

  ASSERT_EQ(pep::ReadFileIfExists(pathUnexistingFile), std::nullopt);

  ASSERT_THROW(pep::ReadFile(pathUnexistingFile), std::runtime_error);
  ASSERT_THROW(pep::ReadFile(pathUnexistingFile.c_str()), std::runtime_error);
  ASSERT_THROW(pep::ReadFile(std::filesystem::path(pathUnexistingFile)), std::runtime_error);
}

}
