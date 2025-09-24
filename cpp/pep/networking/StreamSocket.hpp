#pragma once

#include <functional>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>

namespace pep::networking {

/*!
* \brief Abstracts operations on heterogeneous Boost stream socket types into a non-template interface.
*/
class StreamSocket {
public:
  using Handler = std::function<void(const boost::system::error_code&, std::size_t)>;

private:
  std::function<void(void*, size_t, const Handler&)> mAsyncRead;
  std::function<void(boost::asio::streambuf&, const char*, const Handler&)> mAsyncReadUntil;
  std::function<void(const void*, size_t, const Handler&)> mAsyncWrite;

public:
  /*!
   * \brief Constructs a new instance for the specified Boost stream socket.
   * \tparam TSocket The Boost stream socket's type, e.g. boost::asio::ip::tcp::socket or boost::asio::ssl::stream<boost::asio::ip::tcp::socket>.
   * \param implementor The Boost stream socket instance to wrap.
   * \remark Caller must ensure that the "implementor" Boost socket outlives the StreamSocket instance (and any copies that may be created).
   */
  template <typename TSocket>
  explicit StreamSocket(TSocket& implementor)
    : mAsyncRead([&implementor](void* buffer, size_t bytes, const Handler& handler) { boost::asio::async_read(implementor, boost::asio::buffer(buffer, bytes), boost::asio::transfer_exactly(bytes), handler); }),
    mAsyncReadUntil([&implementor](boost::asio::streambuf& buffer, const char* delimiter, const Handler& handler) { boost::asio::async_read_until(implementor, buffer, delimiter, handler); }),
    mAsyncWrite([&implementor](const void* buffer, size_t bytes, const Handler& handler) { boost::asio::async_write(implementor, boost::asio::buffer(buffer, bytes), handler); }) {
  }

  /*!
  * \brief Asynchronously reads (receives) data from the socket, placing it into a caller-provided buffer.
  * \param buffer The buffer into which received data are copied.
  * \param bytes The number of bytes to write into the buffer. Caller must ensure that the buffer provides sufficient capacity.
  * \param handler A callback function that's invoked when the data has been received, or when the operation has failed.
  * \remark Caller must ensure that the StreamSocket instance and the "buffer" parameter remain valid for the duration of the operation, i.e. until the "handler" has been invoked.
  */
  void asyncRead(void* buffer, size_t bytes, const Handler& handler) { mAsyncRead(buffer, bytes, handler); }

  /*!
  * \brief Asynchronously reads (receives) data from the socket until it contains a specified delimiter, placing received data into a caller-provided buffer.
  * \param buffer The buffer into which received data are placed.
  * \param delimiter Data will be placed into the buffer until it contains this byte sequence.
  * \param handler A callback function that's invoked when data (including the delimiter) has been received, or when the operation has failed.
  * \remark Caller must ensure that the StreamSocket instance and the "buffer" and "delimiter" parameters remain valid for the duration of the operation, i.e. until the "handler" has been invoked.
  * \remark This method may place excess data into the buffer _after_ the specified delimiter. Check the SocketReadBuffer class for a way to deal with this.
  */
  void asyncReadUntil(boost::asio::streambuf& buffer, const char* delimiter, const Handler& handler) { mAsyncReadUntil(buffer, delimiter, handler); }

  /*!
  * \brief Asynchronously writes (sends) data from a caller-provided buffer to the socket.
  * \param buffer A buffer containing the data to send.
  * \param bytes The number of bytes to send from the buffer. Caller must ensure that the buffer provides sufficient capacity.
  * \param handler A callback function that's invoked when the data has been sent, or when the operation has failed.
  * \remark Caller must ensure that the StreamSocket instance and the "buffer" parameter remain valid for the duration of the operation, i.e. until the "handler" has been invoked.
  */
  void asyncWrite(const void* buffer, size_t bytes, const Handler& handler) { mAsyncWrite(buffer, bytes, handler); }
};

}
