#include <pep/async/AsioReadBuffer.hpp>
#include <pep/utils/MiscUtil.hpp>

namespace pep {

AsioReadBuffer::AsioReadBuffer() {
}

void AsioReadBuffer::setSocket(std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> socket) {
  mSocket = socket;
  mClientBuffer.clear();
}

std::string AsioReadBuffer::readSocketBuffer() {
  auto size = mSocketBuffer.size();

  std::string result;
  result.resize(size);

  std::istream source(&mSocketBuffer);
  source.read(&result[0], static_cast<std::streamsize>(size));

  return result;
}

std::string AsioReadBuffer::extractClientBytes(size_t bytes) {
  assert(bytes != 0U);
  assert(bytes <= mClientBuffer.size());

  std::string result;
  if (mClientBuffer.size() == bytes) {
    std::swap(mClientBuffer, result);
  }
  else {
    result = mClientBuffer.substr(0, bytes);
    mClientBuffer = mClientBuffer.substr(bytes);
  }

  return result;
}

void AsioReadBuffer::asyncReadUntil(const std::string& delimiter, const ReadHandler& handle) {
  const auto delimiterLength = delimiter.size();

  // ******* CASE 1: if we buffered (data including) the requested delimiter earlier, return it from the buffer
  auto index = mClientBuffer.find(delimiter);
  if (index != std::string::npos) {
    auto cut = index + delimiterLength;
    return handle(boost::system::error_code(), extractClientBytes(cut));
  }

  // ******* CASE 2: if we buffered data ending with a partial delimiter, find the remainder of the delimiter.
  /* E.g. when the delimiter is "\r\n" and we've already buffered the '\r', we read until the next '\n'. It'll
   * - either be read as the next byte: we indeed had half a delimiter.
   * - or be read later:
   *   - either as part of a (new; separate) '\r\n' delimiter, which is what the caller wanted us to find.
   *   - or as a standalone '\n' (i.e. not part of a delimiter at all).
   * - or not be read at all: the socket just doesn't produce the requested delimiter.
   * In all cases we just buffer the data that we receive, then recursively call this method to deal with the new
   * state of affairs.
   */
  if (auto bufferedPart = FindLongestPrefixAtEnd(mClientBuffer, delimiter)) {
    assert(bufferedPart < delimiterLength); // Otherwise it would have been found by the call to mClientBuffer.find (in case 1)
    auto remainder = std::make_shared<std::string>(delimiter.substr(bufferedPart)); // Ensure the string passed to asio survives until our callback is invoked
    return boost::asio::async_read_until(*mSocket, mSocketBuffer, remainder->c_str(), [self = SharedFrom(*this), remainder, delimiter, handle](const boost::system::error_code& error, std::size_t bytes_transferred) {
      if (error) {
        return handle(error, std::string());
      }

      // Don't process here (which would introduce duplicate code).
      // Instead simply buffer what we received, then let a recursive call to AsioReadBuffer::asyncReadUntil deal with the new buffer state.
      auto received = self->readSocketBuffer();
      self->mClientBuffer.append(received);
      return self->asyncReadUntil(delimiter, handle);
    });
  }

  // ******* CASE 3: we don't have a (full or partial) delimiter yet: read from the socket until we find it
  auto terminator = std::make_shared<std::string>(delimiter); // Ensure the string passed to asio survives until our callback is invoked
  boost::asio::async_read_until(*mSocket, mSocketBuffer, terminator->c_str(), [self = SharedFrom(*this), terminator, delimiterLength, handle](const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (error) {
      return handle(error, std::string());
    }

    auto received = self->readSocketBuffer();

    // Find the delimiter in the received data
    auto index = received.find(*terminator);
    assert(index != std::string::npos);

    // Requested data consists of what we buffered earlier, plus what we received now up to (and including) the delimiter
    auto cut = index + delimiterLength;
    auto requested = std::move(self->mClientBuffer) + received.substr(0, cut);

    // If additional data were received, store them in our buffer for a next call
    self->mClientBuffer = received.substr(cut);

    // Pass the requested data (up to and including the delimiter) to the requestor
    handle(boost::system::error_code(), requested);
  });
}

void AsioReadBuffer::asyncRead(size_t bytes, const ReadHandler& handle) {
  // If we received the requested data earlier, return it from our buffer
  auto buffered = mClientBuffer.size();
  if (buffered >= bytes) {
    return handle(boost::system::error_code(), extractClientBytes(bytes));
  }

  auto required = bytes - buffered;
  boost::asio::async_read(*mSocket, mSocketBuffer, boost::asio::transfer_exactly(required), [self = SharedFrom(*this), required, handle](const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (error) {
      return handle(error, std::string());
    }

    // Read the data we received
    auto received = self->readSocketBuffer();

    // Requested data consists of what we buffered earlier, plus (the first N bytes of) what we received now
    auto requested = std::move(self->mClientBuffer) + received.substr(0, required);

    // If additional data were received, store them in our buffer for a next call
    self->mClientBuffer = received.substr(required);

    // Pass the requested data (containing exactly the requested number of bytes) to the requestor
    handle(boost::system::error_code(), requested);
  });
}

}
