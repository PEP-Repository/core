#include <pep/networking/SendHttpRequest.hpp>

using namespace pep;

// Emscripten proxies TCP over WebSockets, so we'd get WebSocket->TLS->HTTP with the HTTPSClient implementation.
// Instead, we use Web APIs to do an HTTP request here.
#ifdef __EMSCRIPTEN__

#include <pep/async/CreateObservable.hpp>
#include <pep/async/OnAsio.hpp>
#include <pep/utils/Defer.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/CollectionUtils.hpp>

#include <cassert>
#include <span>
#include <stdexcept>
#include <vector>
#include <utility>

#include <emscripten/fetch.h>

using namespace std::literals;

namespace {
const std::string LOG_TAG = "HTTPSClient.Emscripten";
}

rxcpp::observable<HTTPResponse> pep::SendHttpRequest(
  HTTPRequest request,
  std::shared_ptr<boost::asio::io_context> io_context,
  const std::optional<std::filesystem::path> &caCertFilepath
) {
  if (caCertFilepath) {
    LOG(LOG_TAG, warning) << "Custom CA certificate not supported for browser HTTP requests, "
                             "you may need to install it in the OS or bypass checks";
  }

  return CreateObservable<HTTPResponse>(
    [request = std::move(request)](rxcpp::subscriber<HTTPResponse> subscriber) {
      emscripten_fetch_attr_t attr;
      emscripten_fetch_attr_init(&attr);
      // We need to do a synchronous request, because otherwise a thread blocking on io_context::run() will
      // never return to JS, which means the callbacks will never be called.
      // (Alternatively, we could do the request on the main thread.)
      attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_SYNCHRONOUS;

      CheckedCopy(request.getMethod().toString(), attr.requestMethod);

      // Keep pointers valid until emscripten_fetch call
      auto headers = request.getHeaders();
      // Cannot set forbidden headers explicitly (will be set by the browser),
      // so unset them if they exist
      (void) headers.erase("Content-Length");
      (void) headers.erase("Host");
      // Keys & values terminated bt nullptr entry
      std::vector<const char *> headerPtrs;
      headerPtrs.reserve(headers.size() * 2 + 1);
      for (const auto &[key, value]: headers) {
        headerPtrs.push_back(key.c_str());
        headerPtrs.push_back(value.c_str());
      }
      headerPtrs.push_back(nullptr);
      attr.requestHeaders = headerPtrs.data();

      struct fetchContext {
        rxcpp::subscriber<HTTPResponse> subscriber;
        std::string body;
      };
      auto *ctx = new fetchContext{
        .subscriber = std::move(subscriber),
        // According to docs, requestData must remain valid throughout the request
        .body = request.getBody(),
      };
      attr.userData = ctx;
      attr.requestData = ctx->body.data();
      attr.requestDataSize = ctx->body.size();

      static constexpr auto closeFetch = [](emscripten_fetch_t *&fetch) {
        delete static_cast<fetchContext *>(std::exchange(fetch->userData, nullptr));
        [[maybe_unused]] auto ret = emscripten_fetch_close(std::exchange(fetch, nullptr));
        assert(ret == EMSCRIPTEN_RESULT_SUCCESS && "Failed to close fetch context");
      };

      attr.onerror = [](emscripten_fetch_t *fetch) noexcept {
        PEP_DEFER(closeFetch(fetch));
        const auto ctx = static_cast<fetchContext *>(fetch->userData);
        rxcpp::observable<>::error<HTTPResponse>(
              std::runtime_error("Failed to fetch URL: "s + fetch->url))
            .subscribe(ctx->subscriber);
      };
      attr.onsuccess = [](emscripten_fetch_t *fetch) noexcept {
        PEP_DEFER(closeFetch(fetch));
        const auto ctx = static_cast<fetchContext *>(fetch->userData);

        try {
          HTTPMessage::HeaderMap headerMap; {
            /// Header bytes including NULL terminator
            auto headerBytes = emscripten_fetch_get_response_headers_length(fetch);
            if (!headerBytes) {
              throw std::runtime_error("HTTP response headers not available");
            }
            std::string headersStr(headerBytes - 1, '\0');
            auto headerBytes2 = emscripten_fetch_get_response_headers(fetch, headersStr.data(), headersStr.size() + 1);
            if (headerBytes2 != headerBytes) {
              throw std::runtime_error("Failed to get HTTP response headers");
            }

            const auto headersPtr = emscripten_fetch_unpack_response_headers(headersStr.c_str());
            PEP_DEFER(emscripten_fetch_free_unpacked_response_headers(headersPtr));
            if (!headersPtr) {
              throw std::runtime_error("Failed to split HTTP response headers");
            }

            for (auto headerIt = headersPtr; *headerIt; headerIt += 2) {
              const char *key = headerIt[0], *value = headerIt[1];
              headerMap[key] = value;
            }
          }

          rxcpp::observable<>::from(HTTPResponse(
            fetch->status, fetch->statusText,
            std::string(fetch->data, static_cast<size_t>(fetch->numBytes)),
            std::move(headerMap), false
          )).subscribe(ctx->subscriber);
        } catch (...) {
          rxcpp::observable<>::error<HTTPResponse>(std::current_exception())
              .subscribe(ctx->subscriber);
        }
      };

      LOG(LOG_TAG, debug) << "Fetching URL: " << request.uri();
      // Note: all parameters get copied in emscripten_fetch
      auto fetch = emscripten_fetch(&attr, std::string(request.uri().buffer()).c_str());
      // Make sure that code isn't changed to prematurely discard these variables, which must remain valid until the above function call
      (void) headers;
      (void) headerPtrs;
      (void) ctx;
      if (!fetch) {
        throw std::runtime_error("Failed to initiate HTTP fetch");
      }
    })
    .subscribe_on(observe_on_asio(*io_context));
}

#else // ! __EMSCRIPTEN__

#include <pep/networking/HttpClient.hpp>

#include <rxcpp/operators/rx-finally.hpp>

rxcpp::observable<HTTPResponse> pep::SendHttpRequest(HTTPRequest request, std::shared_ptr<boost::asio::io_context> io_context, const std::optional<std::filesystem::path>& caCertFilepath) {
  networking::HttpClient::Parameters parameters(*io_context, request.uri());
  parameters.caCertFilepath(caCertFilepath);
  auto client = networking::HttpClient::Create(std::move(parameters));
  client->start();
  return client->sendRequest(std::move(request))
    .finally([client] {
      client->shutdown();
    });
}

#endif
