#pragma once
#include <pep/async/IoContext_fwd.hpp>
#include <pep/networking/HTTPMessage.hpp>

#include <filesystem>
#include <memory>
#include <optional>

#include <rxcpp/rx-observable-fwd.hpp>

namespace pep {
/*!
 * \brief Send an HTTP request. Compatible with Emscripten.
 * \param request The Request to send
 * \param io_context The io_context to run the request on
 * \return Observable that, if no error occurs, emits exactly one Response
 */
rxcpp::observable<HTTPResponse> SendHttpRequest(HTTPRequest request, std::shared_ptr<boost::asio::io_context> io_context, const std::optional<std::filesystem::path>& caCertFilepath = std::nullopt);
}
