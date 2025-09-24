#include <pep/networking/SocketReadBuffer.hpp>
#include <pep/utils/CollectionUtils.hpp>
#include <istream>

namespace pep::networking {

SocketReadBuffer::SocketReadBuffer()
  : mAsyncRead(&SocketReadBuffer::asyncReadDirectly) {
}

std::string SocketReadBuffer::readSocketBuffer() {
  auto size = mSocketBuffer.size();

  std::string result(size, '\0');

  std::istream source(&mSocketBuffer);
  source.read(result.data(), static_cast<std::streamsize>(result.size()));

  return result;
}

void SocketReadBuffer::asyncReadUntil(StreamSocket& source, const char* delimiter, const DelimitedReadHandler& handle) {
  assert(delimiter != nullptr);
  const auto delimiterLength = strlen(delimiter);

  // ******* CASE 1: if we buffered (data including) the requested delimiter earlier, return it from the buffer
  auto index = mClientBuffer.find(delimiter);
  if (index != std::string::npos) {
    auto bytes = index + delimiterLength;

    // Save ourselves a string copy: we need to return either the start or the entirety of the client buffer
    auto result = std::move(mClientBuffer);

    if (result.size() == bytes) { // Returning the entire buffer contents
      this->clearClientBuffer(); // A.o. ensure that std::move didn't leave our client buffer intact: see e.g. https://stackoverflow.com/a/27376799
    }
    else { // Returning a substring of the (original) buffer contents
      assert(bytes < result.size());
      mClientBuffer = result.substr(bytes); // Copy excess bytes back into the client buffer...
      result.resize(bytes); // ... then crop the result to the requested length
      mAsyncRead = &SocketReadBuffer::asyncReadBuffered; // Next bytes must be read from our buffer
    }

    return handle(boost::system::error_code(), result); // TODO: invoke asynchronously
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
    auto remainder = delimiter + bufferedPart;
    return source.asyncReadUntil(mSocketBuffer, remainder, [self = SharedFrom(*this), &source, delimiter, handle](const boost::system::error_code& error, std::size_t bytes_transferred) {
      auto received = self->readSocketBuffer();
      if (error) {
        // Don't retain received data if an error occurred
        received = self->mClientBuffer + received;
        self->clearClientBuffer();
        return handle(error, received);
      }

      // Don't process here (which would introduce duplicate code).
      // Instead simply buffer what we received, then let a recursive call to SocketReadBuffer::asyncReadUntil deal with the new buffer state.
      self->mClientBuffer.append(received);
      assert(self->mAsyncRead == &SocketReadBuffer::asyncReadBuffered); // Next bytes must be read from our buffer
      return self->asyncReadUntil(source, delimiter, handle);
    });
  }

  // ******* CASE 3: we don't have a (full or partial) delimiter yet: read from the socket until we find it
  return source.asyncReadUntil(mSocketBuffer, delimiter, [self = SharedFrom(*this), delimiter, delimiterLength, handle](const boost::system::error_code& error, std::size_t bytes_transferred) {
    auto received = self->readSocketBuffer();
    if (error) {
      // Don't retain received data if an error occurred
      received = self->mClientBuffer + received;
      self->clearClientBuffer();
      return handle(error, received);
    }

    // Find the delimiter in the received data
    auto index = received.find(delimiter);
    assert(index != std::string::npos);

    // Requested data consists of what we buffered earlier, plus what we received now up to (and including) the delimiter
    auto cut = index + delimiterLength;
    auto requested = std::move(self->mClientBuffer) + received.substr(0, cut);

    // If additional data were received, store them in our client buffer for a next call
    self->mClientBuffer = received.substr(cut);
    self->mAsyncRead = self->mClientBuffer.empty() ? &SocketReadBuffer::asyncReadDirectly : &SocketReadBuffer::asyncReadBuffered;

    // Pass the requested data (up to and including the delimiter) to the requestor
    handle(boost::system::error_code(), requested);
  });
}

void SocketReadBuffer::asyncReadDirectly(StreamSocket& source, void* destination, size_t bytes, const RawReadHandler& handle) {
  source.asyncRead(destination, bytes, handle);
}

void SocketReadBuffer::asyncReadBuffered(StreamSocket& source, void* destination, size_t bytes, const RawReadHandler& handle) {
  assert(destination != nullptr);
  assert(bytes != 0U);

  // Feed the destination what we may have buffered during an earlier invocation of asyncReadUntil
  auto buffered = mClientBuffer.size();
  if (buffered != 0U) {
    auto extract = std::min(buffered, bytes);
    memcpy(destination, mClientBuffer.data(), extract);

    if (buffered == extract) { // Returning the entire buffer contents
      this->clearClientBuffer();
    }
    else { // Returning a substring of the buffer contents
      mClientBuffer = mClientBuffer.substr(extract); // Discard extracted bytes from the client buffer
    }

    if (extract == bytes) { // Buffer contained all requested bytes: no need to read more from the socket
      return handle(boost::system::error_code(), bytes);
    }
  }

  // Buffer didn't contain sufficient bytes: we need to retrieve remaining bytes from the socket
  assert(mAsyncRead == &SocketReadBuffer::asyncReadDirectly);
  this->asyncReadDirectly(source, static_cast<char*>(destination) + buffered, bytes - buffered, [buffered, handle](const boost::system::error_code& error, std::size_t bytes_transferred) {
    // Destination has been filled with (1) what we had buffered earlier plus (2) the bytes that we've now received from the socket
    return handle(error, buffered + bytes_transferred);
    });
}

void SocketReadBuffer::asyncRead(StreamSocket& source, void* destination, size_t bytes, const RawReadHandler& handle) {
  assert(mAsyncRead != nullptr);
  (this->*mAsyncRead)(source, destination, bytes, handle);
}

void SocketReadBuffer::asyncAppendRemaining(std::shared_ptr<std::string> buffer, StreamSocket& source, const DelimitedReadHandler& handle) {
  // Increase the buffer's capacity...
  constexpr size_t CHUNK_SIZE = 4096U;
  auto offset = buffer->size();
  buffer->resize(offset + CHUNK_SIZE);
  // ... then read the next chunk into the buffer section that we just added
  this->asyncRead(source, buffer->data() + offset, CHUNK_SIZE, [buffer, &source, handle, self = SharedFrom(*this)](const boost::system::error_code& error, std::size_t bytes) {
    // Drop buffer capacity that the read action didn't fill
    if (bytes != CHUNK_SIZE) {
      buffer->resize(buffer->size() - CHUNK_SIZE + bytes);
    }

    if (error == make_error_code(boost::asio::error::eof)) { // Finished reading
      handle(boost::system::error_code(), *buffer);
    }
    else if (error) { // An actual error
      handle(error, *buffer);
    }
    else { // Not at EOF yet: keep reading
      self->asyncAppendRemaining(buffer, source, handle);
    }
    });
}

void SocketReadBuffer::asyncReadAll(StreamSocket& source, const DelimitedReadHandler& handle) {
  this->asyncAppendRemaining(std::make_shared<std::string>(), source, handle);
}

void SocketReadBuffer::clearClientBuffer() {
  mClientBuffer.clear();
  mAsyncRead = &SocketReadBuffer::asyncReadDirectly;
}

void SocketReadBuffer::clear() {
  this->clearClientBuffer();
  if (mSocketBuffer.size() != 0U) {
    this->readSocketBuffer();
  }
}

}
