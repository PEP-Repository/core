#pragma once

#include <pep/async/IoContext_fwd.hpp>
#include <pep/networking/HTTPMessage.hpp>
#include <unordered_map>
#include <functional>
#include <memory>

#include <filesystem>
#include <rxcpp/rx-lite.hpp>

struct mg_context; // Forward declares a type provided by CivetWeb

namespace pep {

struct HttpRequestHandlerParams;

class HTTPServer {
public:
  HTTPServer(uint16_t port, std::shared_ptr<boost::asio::io_context> io_context, std::optional<std::filesystem::path> tlsCertificate=std::nullopt);
  ~HTTPServer() noexcept;

  using BasicHandler = std::function<HTTPResponse(const HTTPRequest&, std::string remoteIp)>;
  using ObservableHandler = std::function<rxcpp::observable<HTTPResponse>(const HTTPRequest&, std::string remoteIp)>;
  void registerHandler(const std::string& uri, bool exactMatchOnly, BasicHandler handler, const std::string& method = "");
  void registerHandler(const std::string& uri, bool exactMatchOnly, ObservableHandler handler, const std::string& method = "");
  /// Asynchronously stop server and wait for running handlers to complete.
  /// Do not call other methods after calling this.
  void asyncStop();

private:
  void registerHandlerParams(std::shared_ptr<HttpRequestHandlerParams> params);

  mg_context *ctx_;
  std::unique_ptr<std::unordered_map<std::string, std::shared_ptr<HttpRequestHandlerParams>>> registeredHandlers_;
  std::shared_ptr<boost::asio::io_context> ioContext_;
};

}
