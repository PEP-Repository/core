#include <pep/httpserver/HTTPServer.hpp>

#include <pep/async/OnAsio.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Log.hpp>

#include <civetweb.h>
#include <boost/asio/io_context.hpp>
#include <sstream>

namespace pep {

static const std::string LOG_TAG ("HTTPServer");

SingleWorker HTTPServer::CleanupWorker = SingleWorker();

struct httpRequestHandlerParams {
  std::string method;
  std::string uri;
  bool exactMatchOnly;
  std::shared_ptr<boost::asio::io_context> io_context;

  virtual ~httpRequestHandlerParams() = default;

  virtual rxcpp::observable<HTTPResponse> runHandler(const HTTPRequest& request, std::string remoteIp) = 0;

protected:
  httpRequestHandlerParams(const std::string& method, const std::string& uri, bool exactMatchOnly, std::shared_ptr<boost::asio::io_context> io_context)
    : method(method), uri(uri), exactMatchOnly(exactMatchOnly), io_context(io_context) {}
};

struct httpRequestHandlerParamsBasic : httpRequestHandlerParams {
  HTTPServer::BasicHandler func;

  httpRequestHandlerParamsBasic(HTTPServer::BasicHandler func, const std::string& method, const std::string& uri, bool exactMatchOnly, std::shared_ptr<boost::asio::io_context> io_context)
    : httpRequestHandlerParams(method, uri, exactMatchOnly, io_context), func(func) {}

  rxcpp::observable<HTTPResponse> runHandler(const HTTPRequest& request, std::string remoteIp) override {
    return run_on_asio(*io_context, std::bind(func, request, std::move(remoteIp)));
  }
};

struct httpRequestHandlerParamsObservable : httpRequestHandlerParams {
  HTTPServer::ObservableHandler func;

  httpRequestHandlerParamsObservable(HTTPServer::ObservableHandler func, const std::string& method, const std::string& uri, bool exactMatchOnly, std::shared_ptr<boost::asio::io_context> io_context)
    : httpRequestHandlerParams(method, uri, exactMatchOnly, io_context), func(func) {}

  rxcpp::observable<HTTPResponse> runHandler(const HTTPRequest& request, std::string remoteIp) override {
    return func(request, std::move(remoteIp));
  };
};

static int writeResponse(mg_connection *conn, const pep::HTTPResponse& response) {
  std::string responseString = response.toString();
  if (mg_write(conn, responseString.data(), responseString.length()) < 0) {
    throw std::runtime_error("Failed to write HTTP response");
  }
  return static_cast<int>(response.getStatusCode());
}

static int requestHandler(struct mg_connection *conn, void *cbdata)
{
  httpRequestHandlerParams* params = static_cast<httpRequestHandlerParams*>(cbdata);
  const mg_request_info* requestInfo = mg_get_request_info(conn);

  LOG(LOG_TAG, debug) << "Handler method: " << (params->method.empty() ? "<empty>" : params->method) << ". Request method: " << requestInfo->request_method;
  LOG(LOG_TAG, debug) << "Handler uri: " << params->uri << ". Request uri: " << requestInfo->local_uri;
  LOG(LOG_TAG, debug) << "match uri exactly: " << params->exactMatchOnly;

  if (params->exactMatchOnly && params->uri != requestInfo->local_uri) {
    LOG(LOG_TAG, debug) << "Request handler does not match request.";
    return 0;
  }

  if (!params->method.empty() && params->method != requestInfo->request_method) {
    LOG(LOG_TAG, debug) << "Wrong method.";
    return writeResponse(conn, HTTPResponse("405 Method Not Allowed", "Expected " + params->method + " request"));
  }

  LOG(LOG_TAG, debug) << "Request handler matches request. Start handling the request";
  try {
    std::map<std::string, std::string, CaseInsensitiveCompare> headers;
    for(size_t i = 0; i < static_cast<size_t>(requestInfo->num_headers); ++i) {
      const auto& [it, isNew] = headers.try_emplace(requestInfo->http_headers[i].name, requestInfo->http_headers[i].value);
      if(!isNew) {
        it->second = it->second + "," + requestInfo->http_headers[i].value; //Multiple occurrences of the same header can be combined, separated by comma's see https://tools.ietf.org/html/rfc2616#section-4.2
      }
    }
    auto hostHeader = headers.find("Host");
    if(hostHeader == headers.end()) {
      return writeResponse(conn, HTTPResponse("400 Bad Request", "The HTTP request did not have a Host header."));
    }

    std::string body;
    if(requestInfo->content_length > 0) {
      body = std::string(static_cast<std::string::size_type>(requestInfo->content_length), '\0');
      if (mg_read(conn, body.data(), body.size()) < 0) {
        throw std::runtime_error("Failed to read HTTP request body");
      }
    }
    std::string queryString;
    if(requestInfo->query_string)
      queryString=requestInfo->query_string;
    HTTPRequest request(hostHeader->second, networking::HttpMethod::FromString(requestInfo->request_method),
      boost::urls::url(requestInfo->local_uri).set_encoded_query(queryString),
      body, headers, false);


    // We first check whether io_context is still running, and then we run the request handler on it.
    // Since we are multithreaded here, io_context can stop between those two steps.
    // So we add a work_guard, to make sure it keeps running, even if it runs out of work.
    // workGuard will be active until it goes out of scope.
    [[maybe_unused]] auto workGuard = boost::asio::make_work_guard(*params->io_context);
    if(params->io_context->stopped()) {
      // Since io_context is no longer running, the application is already being closed.
      // We want to handle it as gracefully as possible the application doesn't e.g. segfault
      // Using LOG can already lead to a segfault. Civetweb can however still write a response.
      return writeResponse(conn, HTTPResponse("500 Internal Server Error", "Error: application is closing. Can no longer handle requests."));
    }
    return writeResponse(conn, params->runHandler(request, requestInfo->remote_addr).as_blocking().first());
  }
  catch (std::exception& e) {
    LOG(LOG_TAG, pep::error) << "Unexpected error while handling request: " << e.what();
    return writeResponse(conn, HTTPResponse("500 Internal Server Error", "Internal Server error"));
  }
}

HTTPServer::HTTPServer(uint16_t port, std::shared_ptr<boost::asio::io_context> io_context, std::optional<std::filesystem::path> tlsCertificate)
  : mRegisteredHandlers(std::make_unique<decltype(mRegisteredHandlers)::element_type>()), mIoContext(io_context) {
  auto portStr = std::to_string(port);
  std::vector<const char*> options;
  [[maybe_unused]] std::string tlsCertificateStr; // Needs to remain valid until mg_start2 call
  if(tlsCertificate) {
#ifdef HTTPSERVER_WITH_TLS
    auto canonical = std::filesystem::canonical(*tlsCertificate);
    if(!std::filesystem::is_regular_file(canonical)) {
      throw std::runtime_error(canonical.string() + " is not a file");
    }
    portStr += "s";

    //std::filesystem::path::c_str() returns `const wchar_t*` on Windows. So we first convert it to a string, and take the c_str() of that.
    tlsCertificateStr = tlsCertificate->string();
    options.push_back("ssl_certificate");
    options.push_back(tlsCertificateStr.c_str());
#else
    throw std::runtime_error("HTTPServer is constructed with a TLS Certificate set, but HTTPServer was built without TLS support.");
#endif
  }
  options.push_back("listening_ports");
  options.push_back(portStr.c_str());
  options.push_back(nullptr);

  mg_init_data init{};
  init.configuration_options = options.data();

  std::string startErrorMsgBuf(256, '\0');
  mg_error_data startError{};
  startError.text = startErrorMsgBuf.data();
  startError.text_buffer_size = startErrorMsgBuf.size();

  mCtx = mg_start2(&init, &startError);
  // Make sure that code isn't changed to prematurely discard these variables, which must remain valid until the above function call
  (void) options;
  (void) portStr;
  (void) tlsCertificateStr;

  if(!mCtx) {
    throw std::runtime_error("Could not start web server on port " + std::to_string(port) + ": " + startError.text);
  }
  // Make sure that code isn't changed to prematurely discard this variable, which must remain valid until the above throw statement
  (void)startErrorMsgBuf;
}

HTTPServer::~HTTPServer() noexcept {
  asyncStop();
}

void HTTPServer::registerHandler(const std::string& uri, bool exactMatchOnly, BasicHandler handler, const std::string& method) {
  auto handlerParams = std::make_shared<httpRequestHandlerParamsBasic>(handler, method, uri, exactMatchOnly, mIoContext);
  registerHandlerParams(handlerParams);
}

void HTTPServer::registerHandler(const std::string& uri, bool exactMatchOnly, ObservableHandler handler, const std::string& method) {
  auto handlerParams = std::make_shared<httpRequestHandlerParamsObservable>(handler, method, uri, exactMatchOnly, mIoContext);
  registerHandlerParams(handlerParams);
}

void HTTPServer::asyncStop() {
  if (!mCtx) {
    return;
  }
  LOG(LOG_TAG, debug) << "Stopping server " << mCtx;

  //We don't want to block on the call to mg_stop, because:
  // 1. This method is usually called from the main thread
  // 2. mg_stop waits for all civetweb worker threads to finish handling any requests
  // 3. When handling a request, we schedule the handler on mIoContext, which runs on the main thread, and then use `as_blocking` to wait for the result
  // So: we have mg_stop blocking the main thread, which will therefore never handle the request on which mg_stop is waiting.
  // We also capture mRegisteredHandlers, in order to make sure the httpRequestHandlerParams it contains are not cleaned up before all ongoing request handlers have finished
  CleanupWorker.doWork([ctx = std::exchange(mCtx, {}), registeredHandlers = std::exchange(mRegisteredHandlers, {})]{
    mg_stop(ctx);
    // I'd like to log here that we're stopped, but the logger may actually be destructed here
  });
}

void HTTPServer::registerHandlerParams(std::shared_ptr<httpRequestHandlerParams> params) {
  if(!mRegisteredHandlers->try_emplace(params->uri, params).second) {
    std::ostringstream msg;
    msg << "A handler for uri " << params->uri << "is already registered";
    throw std::runtime_error(msg.str());
  }

  mg_set_request_handler(mCtx, params->uri.c_str(), requestHandler, params.get());
}

} // namespace pep
