#include <fstream>
#include <random>

#include <gtest/gtest.h>

#include <pep/utils/File.hpp>
#include <pep/utils/Filesystem.hpp>
#include <pep/utils/Random.hpp>
#include <pep/archiving/Pseudonymiser.hpp>
#include <pep/archiving/Tar.hpp>


namespace {

using Path = std::filesystem::path;

std::vector<size_t> GeneratePositions(size_t size, size_t pseudonymLength) {
  std::vector<size_t> positions{};
  size_t cursor{0};

  std::default_random_engine rng(std::random_device{}());
  std::uniform_int_distribution<size_t> dist(0, size - 1);
  for (;;) {
    // Make a jump and add that position to the output. Add at least the pseudonymLength to make sure there will be no overlapping issues.
    cursor += dist(rng) + pseudonymLength;

    if (cursor + pseudonymLength < size) {
      positions.push_back(cursor);
    }
    else {
      break;
    }
  }
  return positions;
}

void InsertPseudonym(std::string& data, const std::string& pseudonym, const std::vector<size_t>& positions) {
  // Add oldPseudonym in various places.
  for (size_t pos : positions) {
    assert(data.size() >= pos + pseudonym.size());
    memcpy(data.data() + pos, pseudonym.c_str(), pseudonym.size());
  }
}

void CreateFile(const Path& path, const std::string& contents) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path.string(), std::ios::out | std::ios::binary);
  out.write(contents.data(), static_cast<std::streamsize>(contents.length()));
  out.close();
}

void CreateFilePair(const Path& inputPath, const Path& expectedPath, const std::string& oldPseudonym, const std::string& newPseudonym, size_t fileSize) {
  std::vector<size_t> positions = GeneratePositions(fileSize, oldPseudonym.length());
  std::string binaryContent = pep::RandomString(fileSize);
  InsertPseudonym(binaryContent, oldPseudonym, positions);
  CreateFile(inputPath, binaryContent);
  InsertPseudonym(binaryContent, newPseudonym, positions);
  CreateFile(expectedPath, binaryContent);
}


class PseudonymiserTest : public testing::Test {
public:
  static const std::string oldPseudonym, placeholder, newLongerPseudonym, newShorterPseudonym;
  static const std::string textContentOldPseudonym, textContentPlaceholder, textContentNewLongerPseudonym, textContentNewShorterPseudonym;

  static const pep::filesystem::Temporary testDirectory;
  static const Path& basePath;
  static const Path oldPseudonymFilename, placeholderFilename, newLongerPseudonymFilename, newShorterPseudonymFilename;
  static const Path binaryPathOldPseudonym, binaryPathPlaceholder;

protected:
  static void SetUpTestSuite() {
    // Simple text files.
    CreateFile(oldPseudonymFilename, textContentOldPseudonym);
    CreateFile(placeholderFilename, textContentPlaceholder);
    CreateFile(newLongerPseudonymFilename, textContentNewLongerPseudonym);
    CreateFile(newShorterPseudonymFilename, textContentNewShorterPseudonym);

    // Simple binary files
    CreateFilePair(binaryPathOldPseudonym, binaryPathPlaceholder, oldPseudonym, placeholder, static_cast<size_t>(1024 * 1024));
  }
};


const std::string PseudonymiserTest::oldPseudonym{"oldPseudonym"};
const std::string PseudonymiserTest::placeholder{pep::Pseudonymiser::GetDefaultPlaceholder().substr(0, PseudonymiserTest::oldPseudonym.length())};
const std::string PseudonymiserTest::newLongerPseudonym{"newLongerPseudonym"};
const std::string PseudonymiserTest::newShorterPseudonym{"shortPseu"};


const std::string PseudonymiserTest::textContentOldPseudonym{"The text of the file, including the " + PseudonymiserTest::oldPseudonym + " in it."};
const std::string PseudonymiserTest::textContentPlaceholder{"The text of the file, including the " + PseudonymiserTest::placeholder + " in it."};
const std::string PseudonymiserTest::textContentNewLongerPseudonym{"The text of the file, including the " + PseudonymiserTest::newLongerPseudonym + " in it."};
const std::string PseudonymiserTest::textContentNewShorterPseudonym{"The text of the file, including the " + PseudonymiserTest::newShorterPseudonym + " in it."};

const pep::filesystem::Temporary PseudonymiserTest::testDirectory{
    std::filesystem::temp_directory_path() / pep::filesystem::RandomizedName("pepTest-pseudonymiser-%%%%-%%%%-%%%%")};
const Path& PseudonymiserTest::basePath{PseudonymiserTest::testDirectory.path()};
const Path PseudonymiserTest::oldPseudonymFilename{basePath / ("file_with_" + PseudonymiserTest::oldPseudonym + "_in_it.txt")};
const Path PseudonymiserTest::placeholderFilename{basePath / ("file_with_" + PseudonymiserTest::placeholder + "_in_it.txt")};
const Path PseudonymiserTest::newLongerPseudonymFilename{basePath / ("file_with_" + PseudonymiserTest::newLongerPseudonym + "_in_it.txt")};
const Path PseudonymiserTest::newShorterPseudonymFilename{basePath / ("file_with_" + PseudonymiserTest::newShorterPseudonym + "_in_it.txt")};
const Path PseudonymiserTest::binaryPathOldPseudonym{PseudonymiserTest::basePath / ("Binary_" + PseudonymiserTest::oldPseudonym + ".bin")};
const Path PseudonymiserTest::binaryPathPlaceholder{PseudonymiserTest::basePath / ("Binary_" + PseudonymiserTest::placeholder + ".bin")};

TEST_F(PseudonymiserTest, SingleSmallFile) {
  // Arrange
  Path textPathOutput = basePath / "SingleSmallOutput.txt";
  pep::Pseudonymiser ps{oldPseudonym};
  auto in = std::ifstream(oldPseudonymFilename.string(), std::ios::binary);
  auto out = std::ofstream(textPathOutput.string(), std::ios::binary);
  auto writeTostream = [&out](const char* c, const std::streamsize l) {out.write(c, l); out.flush(); };

  // Act
  ps.pseudonymise(in, writeTostream);
  out.close();

  // Assert
  ASSERT_EQ(pep::ReadFile(textPathOutput), pep::ReadFile(placeholderFilename));

}
TEST_F(PseudonymiserTest, ShorterPseudonym) {
  // Arrange
  Path textPathOutput = basePath / "ShorterPseudonym.txt";
  pep::Pseudonymiser ps{oldPseudonym, newShorterPseudonym};
  auto in = std::ifstream(oldPseudonymFilename.string(), std::ios::binary);
  auto out = std::ofstream(textPathOutput.string(), std::ios::binary);
  auto writeTostream = [&out](const char* c, const std::streamsize l) {out.write(c, l); out.flush(); };

  // Act
  ps.pseudonymise(in, writeTostream);
  out.close();

  // Assert
  ASSERT_EQ(pep::ReadFile(textPathOutput), pep::ReadFile(newShorterPseudonymFilename));

}

TEST_F(PseudonymiserTest, LongerPseudonym) {
  // Arrange
  Path textPathOutput = basePath / "LongerPseudonym.txt";
  pep::Pseudonymiser ps{oldPseudonym, newLongerPseudonym};
  auto in = std::ifstream(oldPseudonymFilename.string(), std::ios::binary);
  auto out = std::ofstream(textPathOutput.string(), std::ios::binary);
  auto writeTostream = [&out](const char* c, const std::streamsize l) {out.write(c, l); out.flush(); };

  // Act
  ps.pseudonymise(in, writeTostream);
  out.close();

  // Assert
  ASSERT_EQ(pep::ReadFile(textPathOutput), pep::ReadFile(newLongerPseudonymFilename));

}

TEST_F(PseudonymiserTest, BinaryFile) {
  // Arrange
  Path binaryPathOutput = basePath / "MediumBinaryOutput.bin";
  pep::Pseudonymiser ps{oldPseudonym};
  auto in = std::ifstream(binaryPathOldPseudonym.string(), std::ios::binary);
  auto out = std::ofstream(binaryPathOutput.string(), std::ios::binary);
  auto writeTostream = [&out](const char* c, const std::streamsize l) {out.write(c, l); out.flush(); };

  // Act
  ps.pseudonymise(in, writeTostream);
  out.close();

  // Assert
  ASSERT_EQ(pep::ReadFile(binaryPathOutput), pep::ReadFile(binaryPathPlaceholder));
}

}
