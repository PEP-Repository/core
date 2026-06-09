#include <pep/oauth-client/ConsoleAuthorization.hpp>

#include <iostream>

using namespace pep;

namespace {
// See OAuthProvider
const std::string RedirectUri = "/code";
}

rxcpp::observable<AuthorizationResult> pep::ConsoleAuthorization(
  std::shared_ptr<boost::asio::io_context> io_context,
  OAuthClient::GetAuthorizeUriFn getAuthorizeUri
) {
  std::cerr << "Please open " << getAuthorizeUri(RedirectUri, {}) << " in your browser.\n"
      << "Paste your code here: " << std::flush;
  std::string code;
  std::getline(std::cin, code); //TODO? make async
  return rxcpp::observable<>::from(AuthorizationResult::Success(code));
}
