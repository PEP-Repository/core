#pragma once

#include <pep/oauth-client/OAuthClient.hpp>

namespace pep {

rxcpp::observable<AuthorizationResult> ConsoleAuthorization(
  std::shared_ptr<boost::asio::io_context> io_context,
  OAuthClient::GetAuthorizeUriFn getAuthorizeUri
);

}
