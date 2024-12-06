#include <pep/httpserver/HTTPServer.hpp>

#include <pep/async/OnAsio.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Log.hpp>

#include <civetweb.h>

#include <sstream>

namespace pep {

static const std::string LOG_TAG ("HTTPServer");

SingleWorker HTTPServer::CleanupWorker = SingleWorker();

struct httpRequestHandlerParams {
  HTTPServer::Handler func;
  std::string method;
  std::string uri;
  bool exactMatchOnly;
  std::shared_ptr<boost::asio::io_service> io_service;

  httpRequestHandlerParams(HTTPServer::Handler func, const std::string& method, const std::string& uri, bool exactMatchOnly, std::shared_ptr<boost::asio::io_service> io_service)
    : func(func), method(method), uri(uri), exactMatchOnly(exactMatchOnly), io_service(io_service) {}
};

static int writeResponse(mg_connection *conn, const pep::HTTPResponse& response) {
  std::string responseString = response.toString();
  mg_write(conn, responseString.data(), responseString.length());
  return static_cast<int>(response.getStatusCode());
}

static int requestHandler(struct mg_connection *conn, void *cbdata)
{
  httpRequestHandlerParams* params = static_cast<httpRequestHandlerParams*>(cbdata);
  const mg_request_info* requestInfo = mg_get_request_info(conn);

  LOG(LOG_TAG, debug) << "Handler method: " << (params->method.empty() ? "<empty>" : params->method) << ". Request method: " << requestInfo->request_method;
  LOG(LOG_TAG, debug) << "Handler uri: " << params->uri << ". Request uri: " << requestInfo->local_uri;
  LOG(LOG_TAG, debug) << "match uri exactly: " << params->exactMatchOnly;

  if((params->method.empty() || params->method == requestInfo->request_method)
    && (params->uri == requestInfo->local_uri || !params->exactMatchOnly)){
    LOG(LOG_TAG, debug) << "Request handler matches request. Start handling the request";
    try {
      std::map<std::string, std::string, CaseInsensitiveCompare> headers;
      for(size_t i = 0; i < static_cast<size_t>(requestInfo->num_headers); ++i) {
        const auto& [it, isNew] = headers.try_emplace(requestInfo->http_headers[i].name, requestInfo->http_headers[i].value);
        if(!isNew) {
          it->second = it->second + "," + requestInfo->http_headers[i].value; //Multiple occurences of the same header can be combined, separated by comma's see https://tools.ietf.org/html/rfc2616#section-4.2
        }
      }
      auto hostHeader = headers.find("Host");
      if(hostHeader == headers.end()) {
        return writeResponse(conn, HTTPResponse("400 Bad Request", "The HTTP request did not have a Host header."));
      }

      std::string body;
      if(requestInfo->content_length > 0) {
        body = std::string(static_cast<std::string::size_type>(requestInfo->content_length), '\0');
        mg_read(conn, body.data(), body.size());
      }
      std::string queryString;
      if(requestInfo->query_string)
        queryString=requestInfo->query_string;
      HTTPRequest request(hostHeader->second, requestInfo->request_method, requestInfo->local_uri, body, headers, queryString, false);


      // We first check whether io_service is still running, and then we run the request handler on it.
      // Since we are multithreaded here, io_service can stop between those two steps.
      // So we add a work_guard, to make sure it keeps running, even if it runs out of work.
      // workGuard will be active until it goes out of scope.
      [[maybe_unused]] auto workGuard = boost::asio::make_work_guard(*params->io_service);
      if(params->io_service->stopped()) {
        // Since io_service is no longer running, the application is already being closed.
        // We want to handle it as gracefully as possible the application doesn't e.g. segfault
        // Using LOG can already lead to a segfault. Civetweb can however still write a response.
        return writeResponse(conn, HTTPResponse("500 Internal Server Error", "Error: application is closing. Can no longer handle requests."));
      }
      return writeResponse(conn, run_on_asio(*params->io_service, std::bind(params->func, request)).as_blocking().first());
    }
    catch (std::exception& e) {
      LOG(LOG_TAG, pep::error) << "Unexpected error while handling request: " << e.what();
      return writeResponse(conn, HTTPResponse("500 Internal Server Error", "Internal Server error"));
    }
  }
  else {
    LOG(LOG_TAG, debug) << "Request handler does not match request.";
    return 0;
  }
}

HTTPServer::HTTPServer(unsigned short port, std::shared_ptr<boost::asio::io_service> io_service, std::optional<std::filesystem::path> tlsCertificate) : mIoService(io_service) {
  auto portStr = std::to_string(port);
  std::vector<const char*> options;
  std::string tlsCertificateStr;
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
  mCtx = mg_start(nullptr, nullptr, options.data());
  if(!mCtx) {
    throw std::runtime_error("Could not start web server");
  }
}

HTTPServer::~HTTPServer() noexcept {
  try {
    stop();
  } catch (...) {
    Terminate(std::current_exception());
  }
}

void HTTPServer::registerHandler(const std::string& uri, bool exactMatchOnly, Handler handler, const std::string& method) {
  auto handlerParams = std::make_shared<httpRequestHandlerParams>(handler, method, uri, exactMatchOnly, mIoService);
  if(!mRegisteredHandlers.try_emplace(uri, handlerParams).second) {
    std::ostringstream msg;
    msg << "A handler for uri " << uri << "is already registered"; 
    throw std::runtime_error(msg.str());
  }

  mg_set_request_handler(mCtx, uri.c_str(), requestHandler, handlerParams.get());
}

void HTTPServer::stop() {
  //We don't want to block on the call to mg_stop, because:
  // 1. This method is usually called from the main thread
  // 2. mg_stop waits for all civetweb worker threads to finish handling any requests
  // 3. When handling a request, we schedule the handler on mIoService, which runs on the main thread, and then use `as_blocking` to wait for the result
  // So: we have mg_stop blocking the main thread, which will therefore never handle the request on which mg_stop is waiting.
  // We also capture mRegisteredHandlers, in order to make sure the httpRequestHandlerParams it contains are not cleaned up before all ongoing request handlers have finished
  CleanupWorker.doWork([ctx = mCtx, registeredHandlers = std::move(mRegisteredHandlers)](){
    mg_stop(ctx);
  });
}

}
