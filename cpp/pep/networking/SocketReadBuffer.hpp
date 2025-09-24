#pragma once

#include <pep/networking/StreamSocket.hpp>
#include <pep/utils/Shared.hpp>

namespace pep::networking {

/*!
* \brief Frontend for StreamSocket instances that deals with excess data produced by StreamSocket::asyncReadUntil.
*/
class SocketReadBuffer : public std::enable_shared_from_this<SocketReadBuffer>, public SharedConstructor<SocketReadBuffer> { // TODO: don't require shared_ptr use for consuming code that maintains lifetime itself
  friend class SharedConstructor<SocketReadBuffer>;

public:
  using RawReadHandler = StreamSocket::Handler;
  using DelimitedReadHandler = std::function<void(const boost::system::error_code&, const std::string&)>;

private:
  std::string mClientBuffer; // Here we store excess data received from StreamSocket::asyncReadUntil
  boost::asio::streambuf mSocketBuffer; // Helper for calls to StreamSocket::asyncReadUntil; stored as a member variable so that we don't have to heap-allocate a new one for every invocation

  // The method (pointer) to invoke for calls to the "asyncRead" method:
  // - either asyncReadBuffered to deal with excess data that we received and stored earlier in our mClientBuffer,
  // - or asyncReadDirectly from the StreamSocket if the mClientBuffer is empty.
  using AsyncReadMethod = void (SocketReadBuffer::*)(StreamSocket&, void*, size_t, const RawReadHandler&);
  AsyncReadMethod mAsyncRead;

  SocketReadBuffer();

  void asyncReadDirectly(StreamSocket& source, void* destination, size_t bytes, const RawReadHandler& handle);
  void asyncReadBuffered(StreamSocket& source, void* destination, size_t bytes, const RawReadHandler& handle);
  void clearClientBuffer();
  std::string readSocketBuffer();

  void asyncAppendRemaining(std::shared_ptr<std::string> buffer, StreamSocket& source, const DelimitedReadHandler& handle);

public:
  /*!
  * \brief Asynchronously reads (receives) data from the socket until it contains a specified delimiter.
  * \param source The StreamSocket from which to read the data.
  * \param delimiter Data will be placed into the buffer until it contains this byte sequence.
  * \param handle A callback function that's invoked when data (including the delimiter) has been received, or when the operation has failed.
  * \remark Caller must ensure that the "source" and "delimiter" parameters remain valid for the duration of the operation, i.e. until the "handler" has been invoked.
  * \remark If the operation completes successfully, the received data will end with the first occurrence of the delimiter.
  *         Any excess data received from the "source" will be buffered and provided to subsequent calls to asyncRead and/or asyncReadUntil.
  */
  void asyncReadUntil(StreamSocket& source, const char* delimiter, const DelimitedReadHandler& handle);

  /*!
  * \brief Asynchronously reads (receives) data from the socket, placing it into a caller-provided buffer.
  * \param source The StreamSocket from which to read the data.
  * \param destination The buffer into which received data are copied.
  * \param bytes The number of bytes to write into the buffer. Caller must ensure that the buffer provides sufficient capacity.
  * \param handle A callback function that's invoked when the data has been received, or when the operation has failed.
  * \remark Caller must ensure that the "source" and "buffer" parameters remain valid for the duration of the operation, i.e. until the "handler" has been invoked.
  */
  void asyncRead(StreamSocket& source, void* destination, size_t bytes, const RawReadHandler& handle);

  /*!
  * \brief Asynchronously reads (receives) all data from the socket (until EOF is encountered)
  * \param source The StreamSocket from which to read the data.
  * \param handle A callback function that's invoked when all data has been received, or when the operation has failed.
  * \remark Caller must ensure that the "source" parameter remains valid for the duration of the operation, i.e. until the "handler" has been invoked.
  */
  void asyncReadAll(StreamSocket& source, const DelimitedReadHandler& handle);

  /*!
   * \brief Clears the SocketReadBuffer of any remaining data that was buffered from earlier socket reads.
   */
  void clear();
};

}
