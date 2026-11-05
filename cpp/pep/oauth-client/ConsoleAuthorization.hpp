#pragma once

#include <pep/async/IoContext_fwd.hpp>
#include <pep/oauth-client/OAuthClient.hpp>

#include <memory>

namespace pep {

rxcpp::observable<AuthorizationResult> ConsoleAuthorization(
  std::shared_ptr<boost::asio::io_context> ioContext,
  OAuthClient::GetAuthorizeUriFn getAuthorizeUri
);

}
