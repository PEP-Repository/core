#pragma once

#include <pep/networking/HTTPMessage.hpp>
#include <pep/async/SingleWorker.hpp>
#include <unordered_map>
#include <functional>
#include <memory>

#include <boost/asio/io_service.hpp>
#include <filesystem>


struct mg_context;

namespace pep {

struct httpRequestHandlerParams;

class HTTPServer {
public:
  HTTPServer(unsigned short port, std::shared_ptr<boost::asio::io_service> io_service, std::optional<std::filesystem::path> tlsCertificate=std::nullopt);
  ~HTTPServer() noexcept;

  typedef std::function<HTTPResponse(const HTTPRequest&)> Handler;
  void registerHandler(const std::string& uri, bool exactMatchOnly, Handler handler, const std::string& method = "");

private:
  void stop();

  mg_context *mCtx;
  std::unordered_map<std::string, std::shared_ptr<httpRequestHandlerParams>> mRegisteredHandlers;
  std::shared_ptr<boost::asio::io_service> mIoService;

  static SingleWorker CleanupWorker;
};

}
