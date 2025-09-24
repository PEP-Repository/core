#pragma once

#include <cassert>
#include <ios>
#include <string>

#include <boost/iostreams/char_traits.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/operations.hpp>

namespace pep {
class PseudonymiseInputFilter : public boost::iostreams::multichar_input_filter {
private:
  std::string mOldPseudonym;
  std::string mNewPseudonym;

  std::string mBuffer;
  static const size_t PAGE_SIZE{ 2048 };

  bool mEndOfSource{ false };
  size_t mStartReplacingFrom{ 0 };

public:
  PseudonymiseInputFilter(const std::string& oldPseudonym, const std::string& newPseudonym) :
    mOldPseudonym{ oldPseudonym }, mNewPseudonym{ newPseudonym } {
    assert(mOldPseudonym != mNewPseudonym);
    assert(!mOldPseudonym.empty());
  }

  /* Reads data from source and replaces all instances of the old pseudonym with the new. Uses a fixed length buffer mPreReadFromSource and keeps reading bytes from the source until
   * the requested amount of bytes, plus the length of the old pseudonym are in buffer. After processing, it can then be safely said that the first n bytes do not contain any part of
   * the old pseudonym.
   * \param src: Templated Source object. The data is collected from here.
   * \param s: The char array to which the filtered data will be written.
   * \param n: The amount of chars requested by the caller.
   */
  template<typename Source>
  std::streamsize read(Source& src, char* s, std::streamsize n) {
    // If we reached the end of the file (marked by bool) and the buffer is empty, return EOF, indicating the end of this stream.
    if (mEndOfSource && mBuffer.length() == 0) {
      return EOF;
    }
    std::streamsize amountRequested{ n + static_cast<std::streamsize>(mOldPseudonym.length()) };

    if (!mEndOfSource) {
      assert(n >= 0U);
      // amountRequested is always larger than 0, so casting to size_t is safe.
      if (mBuffer.length() < static_cast<std::size_t>(amountRequested)) { // Only read and process more data when there is not enough in the cache to fill the current call.
        // Read until we have n + length of oldPseudonym chars (including what is already in preread).
        do {
          char page[PAGE_SIZE]{};
          auto amountReceived = boost::iostreams::read(src, page, PAGE_SIZE);

          if (amountReceived == EOF) {
            mEndOfSource = true;
            break;
          }
          if (amountReceived == boost::iostreams::WOULD_BLOCK) {
            // Simply pass the WOULD_BlOCK to the caller. See https://www.boost.org/doc/libs/1_81_0/libs/iostreams/doc/guide/asynchronous.html
            // and https://www.boost.org/doc/libs/1_81_0/libs/iostreams/doc/concepts/blocking.html
            return boost::iostreams::WOULD_BLOCK;
          }
          assert(amountReceived >= 0);
          mBuffer.append(page, static_cast<size_t>(amountReceived));

        } while (mBuffer.length() < static_cast<std::size_t>(amountRequested));
      }
    }

    do {
      mStartReplacingFrom = mBuffer.find(mOldPseudonym, mStartReplacingFrom);
      if (mStartReplacingFrom != std::string::npos) {
        mBuffer.replace(mStartReplacingFrom, mOldPseudonym.length(), mNewPseudonym);
        mStartReplacingFrom += mNewPseudonym.size();
      }
    } while (mStartReplacingFrom != std::string::npos);


    // return either the initially requested n bytes, or all we have left in the buffer.
    assert(n >= 0U);
    auto amountReturned = std::min(static_cast<size_t>(n), mBuffer.length());

    memcpy(s, mBuffer.c_str(), amountReturned);
    // cut off the first amountReturned chars of the buffer. These are the bytes we just sent away.
    memmove(mBuffer.data(), mBuffer.data() + amountReturned, mBuffer.size() - amountReturned);
    mBuffer.resize(mBuffer.size() - amountReturned);

    if (mBuffer.length() >= mOldPseudonym.length()) {
      mStartReplacingFrom = mBuffer.length() - mOldPseudonym.length();
    }
    else {
      mStartReplacingFrom = 0U;
    }

    assert(amountReturned <= static_cast<uintmax_t>(std::numeric_limits<std::streamsize>::max()));
    return static_cast<std::streamsize>(amountReturned);
  }

  /* Clean up and be ready for a new stream*/
  template<typename Source>
  void close(Source& src) {
    mBuffer.clear();
    mEndOfSource = false;
    //explicitly call the base class implementation of close.
    filter::close(src);
  }
};
}
