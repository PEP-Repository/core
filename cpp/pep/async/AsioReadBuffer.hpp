#pragma once

#include <pep/utils/Shared.hpp>

#include <functional>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/system/error_code.hpp>

namespace pep {

class AsioReadBuffer : public std::enable_shared_from_this<AsioReadBuffer>, public SharedConstructor<AsioReadBuffer> {
  friend class SharedConstructor<AsioReadBuffer>;

private:
  std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> mSocket;
  boost::asio::streambuf mSocketBuffer;
  std::string mClientBuffer;

private:
  AsioReadBuffer() = default; // TODO: allow non-heap construction for callers that manage lifetime themselves

  std::string readSocketBuffer();
  std::string extractClientBytes(size_t bytes);

public:
  using ReadHandler = std::function<void(const boost::system::error_code&, const std::string&)>;

  void setSocket(std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> socket);

  void asyncReadUntil(const std::string& delimiter, const ReadHandler& handle);
  void asyncRead(size_t bytes, const ReadHandler& handle);
};

}
