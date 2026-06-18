#pragma once

#include <array>
#include <cassert>
#include <ios>
#include <string>

#include <boost/iostreams/char_traits.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/operations.hpp>

namespace pep {
class PseudonymiseInputFilter : public boost::iostreams::multichar_input_filter {
private:
  std::string oldPseudonym_;
  std::string newPseudonym_;

  std::string buffer_;
  static constexpr size_t PageSize{ 2048 };

  bool endOfSource_{ false };
  size_t startReplacingFrom_{ 0 };

public:
  PseudonymiseInputFilter(const std::string& oldPseudonym, const std::string& newPseudonym) :
    oldPseudonym_{ oldPseudonym }, newPseudonym_{ newPseudonym } {
    assert(oldPseudonym_ != newPseudonym_);
    assert(!oldPseudonym_.empty());
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
    if (endOfSource_ && buffer_.length() == 0) {
      return EOF;
    }
    std::streamsize amountRequested{ n + static_cast<std::streamsize>(oldPseudonym_.length()) };

    if (!endOfSource_) {
      assert(n >= 0);
      // amountRequested is always larger than 0, so casting to size_t is safe.
      if (buffer_.length() < static_cast<std::size_t>(amountRequested)) { // Only read and process more data when there is not enough in the cache to fill the current call.
        // Read until we have n + length of oldPseudonym chars (including what is already in preread).
        do {
          std::array<char, PageSize> page{};
          auto amountReceived = boost::iostreams::read(src, page.data(), page.size());

          if (amountReceived == EOF) {
            endOfSource_ = true;
            break;
          }
          if (amountReceived == boost::iostreams::WOULD_BLOCK) {
            // Simply pass the WOULD_BlOCK to the caller. See https://www.boost.org/doc/libs/1_81_0/libs/iostreams/doc/guide/asynchronous.html
            // and https://www.boost.org/doc/libs/1_81_0/libs/iostreams/doc/concepts/blocking.html
            return boost::iostreams::WOULD_BLOCK;
          }
          assert(amountReceived >= 0);
          buffer_.append(page.data(), static_cast<size_t>(amountReceived));

        } while (buffer_.length() < static_cast<std::size_t>(amountRequested));
      }
    }

    do {
      startReplacingFrom_ = buffer_.find(oldPseudonym_, startReplacingFrom_);
      if (startReplacingFrom_ != std::string::npos) {
        buffer_.replace(startReplacingFrom_, oldPseudonym_.length(), newPseudonym_);
        startReplacingFrom_ += newPseudonym_.size();
      }
    } while (startReplacingFrom_ != std::string::npos);


    // return either the initially requested n bytes, or all we have left in the buffer.
    assert(n >= 0);
    auto amountReturned = std::min(static_cast<size_t>(n), buffer_.length());

    memcpy(s, buffer_.c_str(), amountReturned);
    // cut off the first amountReturned chars of the buffer. These are the bytes we just sent away.
    memmove(buffer_.data(), buffer_.data() + amountReturned, buffer_.size() - amountReturned);
    buffer_.resize(buffer_.size() - amountReturned);

    if (buffer_.length() >= oldPseudonym_.length()) {
      startReplacingFrom_ = buffer_.length() - oldPseudonym_.length();
    }
    else {
      startReplacingFrom_ = 0U;
    }

    assert(amountReturned <= static_cast<uintmax_t>(std::numeric_limits<std::streamsize>::max()));
    return static_cast<std::streamsize>(amountReturned);
  }

  /* Clean up and be ready for a new stream*/
  template<typename Source>
  void close(Source& src) {
    buffer_.clear();
    endOfSource_ = false;
    //explicitly call the base class implementation of close.
    filter::close(src);
  }
};
}
