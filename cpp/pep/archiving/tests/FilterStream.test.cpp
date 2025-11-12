#include <sstream>

#include <boost/iostreams/filtering_stream.hpp>
#include <gtest/gtest.h>
#include <pep/archiving/PseudonymiseInputFilter.hpp>

namespace {

void ReadInputStreamToOutputStream(boost::iostreams::filtering_istream& inputStream, std::ostream& out, const size_t bufferSize = 255U) {
  std::string buffer{};
  buffer.resize(bufferSize);

  for (;;) {
    assert(bufferSize <= static_cast<uintmax_t>(std::numeric_limits<std::streamsize>::max())); // Ensure static_cast (below) doesn't lose bits
    auto amount = boost::iostreams::read(inputStream, buffer.data(), static_cast<std::streamsize>(bufferSize));
    if (amount == boost::iostreams::WOULD_BLOCK) {
      // WOULD_BLOCK is as of yet unimplemented. When it is, we want a heads up.
      throw std::runtime_error("boost::iostreams::WOULD_BLOCK encountered.");
    }
    if (amount != EOF) {
      out.write(buffer.data(), amount);
    }
    else {
      break;
    }
  }

  inputStream.reset();
}

void RunTestWithStringSource(const std::string& input, const std::string& oldValue, const std::string& newValue, const std::string& expected) {
  boost::iostreams::filtering_istream in;
  pep::PseudonymiseInputFilter pseudonymiseInputFilter{ oldValue, newValue };
  std::istringstream source{ input };
  in.push(pseudonymiseInputFilter);
  in.push(source);
  std::ostringstream out;
  ReadInputStreamToOutputStream(in, out);
  std::string actual{ std::move(out).str() };
  ASSERT_EQ(actual, expected);
}

void RunTestWithDirectCall(const std::string& input, const std::string& oldValue, const std::string& newValue, const std::string& expected, size_t bufferSize) {
  std::istringstream source{ input };
  pep::PseudonymiseInputFilter pseudonymiseInputFilter{ oldValue, newValue };
  std::string outputbuffer{};
  outputbuffer.resize(bufferSize);
  auto actualAmount = pseudonymiseInputFilter.read(source, outputbuffer.data(), static_cast<std::streamsize>(bufferSize));
  std::string actual{ outputbuffer.substr(0, static_cast<size_t>(actualAmount)) };
  ASSERT_EQ(actual, expected);
}


TEST(FilterStream, SimpleCall) {
  RunTestWithDirectCall("Old text", "Old", "New", "New text", 8);
}

TEST(FilterStream, LargerBuffSize) {
  RunTestWithDirectCall("Old text", "Old", "New", "New text", 10);
}

TEST(FilterStream, SmallerBuffSize) {
  RunTestWithDirectCall("Old text", "Old", "New", "New te", 6);
}

TEST(FilterStream, BuffSizeSmallerThanOldPseudonym) {
  RunTestWithDirectCall("Old text", "Old", "New", "Ne", 2);
}

TEST(FilterStream, noReplace) {
  RunTestWithStringSource("some text", "Old", "New", "some text");
}

TEST(FilterStream, SingleReplace) {
  RunTestWithStringSource("Old text", "Old", "New", "New text");
}

TEST(FilterStream, multipleReplace) {
  RunTestWithStringSource("Old and some other text OldOld", "Old", "New", "New and some other text NewNew");
}

TEST(FilterStream, replaceCaseSensitivity) {
  RunTestWithStringSource("old", "Old", "New", "old");
}

TEST(FilterStream, replaceEmpty) {
  RunTestWithStringSource("", "Old", "New", "");
}

TEST(FilterStream, endOfFileHasCoincidentalPartialPseudonym) {
  RunTestWithStringSource("Text with partial oldPseudo", "oldPseudonym", "newPseudonym", "Text with partial oldPseudo");
}

TEST(FilterStream, reuseFilter) {
  pep::PseudonymiseInputFilter pseudonymiseInputFilter{ "Old", "New" };

  boost::iostreams::filtering_istream firstStream;
  std::istringstream firstSource{ "Old Text" };
  firstStream.push(pseudonymiseInputFilter);
  firstStream.push(firstSource);

  boost::iostreams::filtering_istream secondStream;
  std::istringstream secondSource{ "Some other Old Text" };
  secondStream.push(pseudonymiseInputFilter);
  secondStream.push(secondSource);

  std::ostringstream out;

  ReadInputStreamToOutputStream(firstStream, out);
  ASSERT_EQ(out.str(), "New Text");
  // clear the outstream of any data
  out.str(std::string());
  // set stream state to goodbit
  out.clear();


  ReadInputStreamToOutputStream(secondStream, out);
  ASSERT_EQ(std::move(out).str(), "Some other New Text");
}

}
