#pragma once

#include <pep/oauth-client/OAuthClient.hpp>

namespace pep {

rxcpp::observable<AuthorizationResult> BrowserAuthorization(
  std::shared_ptr<boost::asio::io_context> io_context,
  OAuthClient::GetAuthorizeUriFn getAuthorizeUri
);

}
