#include <pep/networking/HTTPSClient.hpp>
#include <pep/async/OnAsio.hpp>
#include <pep/async/CreateObservable.hpp>

#include <boost/bind/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>

namespace pep {

static const std::string LOG_TAG ("HTTPSClient");

rxcpp::observable<HTTPResponse> HTTPSClient::Connection::sendRequest(std::shared_ptr<HTTPRequest> request) {
  onRequest.notify(request);
  return CreateObservable<HTTPResponse>([self = SharedFrom(*this), request](rxcpp::subscriber<HTTPResponse> s){
    self->out.push(std::make_pair(request, s));
    self->ensureSend();
  }).subscribe_on(observe_on_asio(*getClient()->getIoContext()));
}



void HTTPSClient::Connection::reconnect() {
  requestActive = false;
  mReadBuffer->setSocket(nullptr);
  TLSClient<TLSProtocol>::Connection::reconnect();
}

void HTTPSClient::Connection::onConnectSuccess() {
  mReadBuffer->setSocket(socket);
  TLSClient<TLSProtocol>::Connection::onConnectSuccess();
  ensureSend();
}

void HTTPSClient::Connection::onError(const boost::system::error_code& error) {
  LOG(LOG_TAG, debug) << "onError: " << error.message();
  mState = TLSProtocol::Connection::FAILED;
  reconnect();
}

void HTTPSClient::Connection::handleWrite(const boost::system::error_code& error, size_t bytes_transferred)  {
  if (error) {
    onError(error);
    return;
  }

  mReadBuffer->asyncReadUntil("\r\n", [self = SharedFrom(*this)](const boost::system::error_code& error, const std::string& received) {
    self->handleReadStatusline(error, received);
  });
}


void HTTPSClient::Connection::handleReadStatusline(const boost::system::error_code& error, const std::string& received) {
  if (error) {
    onError(error);
    return;
  }

  std::istringstream responseStream(received);
  std::string http_version;
  unsigned int statuscode;
  std::string statusMessage;
  responseStream >> http_version;
  responseStream >> statuscode;
  std::getline(responseStream, statusMessage);
  boost::trim_if(statusMessage, boost::is_any_of(" \r\n\t"));
  if (!responseStream || !http_version.starts_with("HTTP/")) {
    LOG(LOG_TAG, severity_level::error) << "Invalid HTTP response\n";
    return; // TODO: notify sender
  }
  response.setStatusCode(statuscode);
  response.setStatusMessage(statusMessage);

  mReadBuffer->asyncReadUntil("\r\n\r\n", [self = SharedFrom(*this)](const boost::system::error_code& error, const std::string& received) {
    self->handleReadHeaders(error, received);
  });
}

void HTTPSClient::Connection::handleReadHeaders(const boost::system::error_code& error, const std::string& received) {
  if (error) {
    onError(error);
    return;
  }

  std::istringstream responseStream(received);
  std::string header;
  // https://datatracker.ietf.org/doc/html/rfc7230#section-3.2
  while (std::getline(responseStream, header) && header != "\r") {
    size_t index = header.find(':');
    if(index != std::string::npos) {
      std::string headerName = header.substr(0, index);
      std::string headerValue = header.substr(index + 1);
      boost::trim_if(headerValue, boost::is_any_of(" \r\n\t"));
      response.setHeader(headerName, headerValue);
    } else {
      LOG(LOG_TAG, warning) << "Ignoring malformed header: " << header << '\n';
    }
  }

  auto transferEncodingHeader = response.getHeaders().find("Transfer-Encoding");
  if(transferEncodingHeader != response.getHeaders().end()) {
    if(transferEncodingHeader->second.find("chunked") != std::string::npos) {
      mReadBuffer->asyncReadUntil("\r\n", [self = SharedFrom(*this)](const boost::system::error_code& error, const std::string& received) {
        self->handleReadChunkSize(error, received);
      });
    }
    // TODO: else?
  } else {
    auto contentLengthHeader = response.getHeaders().find("Content-Length");
    size_t contentlength = 0U;
    if (contentLengthHeader != response.getHeaders().end()) {
      contentlength = boost::lexical_cast<size_t>(contentLengthHeader->second);
    }

    if (contentlength > 0) {
      mReadBuffer->asyncRead(contentlength, [self = SharedFrom(*this)](const boost::system::error_code& error, const std::string& received) {
        self->handleReadBody(error, received);
      });
    } else {
      complete();
    }
  }
}

void HTTPSClient::Connection::handleReadChunkSize(const boost::system::error_code& error, const std::string& received) {
  if (error) {
    onError(error);
    return;
  }

  assert(boost::ends_with(received, "\r\n"));
  std::istringstream responseStream(received.substr(0, received.size() - 2));
  size_t chunkSize;
  responseStream >> std::hex >> chunkSize;

  if(chunkSize > 0) {
    // Read the chunk, including the trailing CRLF
    mReadBuffer->asyncRead(chunkSize + 2, [self = SharedFrom(*this)](const boost::system::error_code& error, const std::string& received) {
      self->handleReadChunk(error, received);
    });
  } else {
    // We're processing the last (empty) chunk: read the CRLF that's written after its (empty) content
    mReadBuffer->asyncRead(2, [self = SharedFrom(*this)](const boost::system::error_code& error, const std::string& received) {
      if (error) {
        return self->onError(error);
      }

      assert(received == "\r\n");
      self->complete();
    });
  }
}

void HTTPSClient::Connection::handleReadChunk(const boost::system::error_code& error, const std::string& received) {
  if (error) {
    onError(error);
    return;
  }

  assert(boost::ends_with(received, "\r\n"));
  response.getBodyparts().push_back(std::make_shared<std::string>(received.substr(0, received.size() - 2U)));

  mReadBuffer->asyncReadUntil("\r\n", [self = SharedFrom(*this)](const boost::system::error_code& error, const std::string& received) {
    self->handleReadChunkSize(error, received);
  });
}

void HTTPSClient::Connection::handleReadBody(const boost::system::error_code& error, const std::string& received) {
  if (error) {
    onError(error);
    return;
  }

  response.getBodyparts().push_back(std::make_shared<std::string>(received));

  complete();
}

void HTTPSClient::Connection::ensureSend() {
  if (mState < TLSProtocol::Connection::HANDSHAKE_DONE || requestActive)
    return;
  if (out.empty())
    return;

  requestActive = true;
  response = HTTPResponse();

  std::shared_ptr<HTTPRequest> request = out.front().first;

  // We use separate buffers for the header and bodyparts of the http request,
  // because that saves us the memcopies needed to put them next to eachother.
  std::vector<boost::asio::const_buffer> buffers;

  auto header = std::make_shared<std::string>(request->headerToString());
  buffers.push_back(boost::asio::const_buffer(header->data(), header->size()));

  for (auto bodypart : request->getBodyparts())
    buffers.push_back(boost::asio::const_buffer(
          bodypart->data(), bodypart->size()));

  // lambda capture 'header' to keep it alive during the async operation
  boost::asio::async_write(*socket, buffers,
    [self = SharedFrom(*this), header](const boost::system::error_code& error, std::size_t bytes_transferred) {
    return self->handleWrite(error, bytes_transferred);
  });
}

void HTTPSClient::Connection::complete() {
  rxcpp::subscriber<HTTPResponse> subscriber = out.front().second;
  out.pop();
  requestActive = false;
  subscriber.on_next(response);
  subscriber.on_completed();
  ensureSend();
}

std::string HTTPSClient::Connection::pathFromUrl(std::string url) {
  auto& endPoint = getEndPoint();

  std::string root = "https://" + endPoint.hostname;
  if (endPoint.port != 443) {
    root += ':' + std::to_string(endPoint.port);
  }
  root += basePath;

  if (boost::algorithm::istarts_with(url, root)) {
    return url.substr(root.length());
  }

  throw std::runtime_error("Url " + url + " doesn't match current https connection to " + root);
}

rxcpp::observable<HTTPResponse> HTTPSClient::SendRequest(std::shared_ptr<HTTPRequest> request, std::shared_ptr<boost::asio::io_context> io_context, const std::optional<std::filesystem::path>& caCertFilepath) {
  assert(request->uri().scheme() == "https");
  auto params = std::make_shared<Parameters>();
  params->setEndPoint(EndPoint(request->getHost(), request->uri().has_port() ? request->uri().port_number() : 443));
  params->setIoContext(io_context);
  if(caCertFilepath) {
    params->setCaCertFilepath(*caCertFilepath);
  }
  auto connection = CreateTLSClientConnection<HTTPSClient>(params);
  return connection->sendRequest(request);
}

}
