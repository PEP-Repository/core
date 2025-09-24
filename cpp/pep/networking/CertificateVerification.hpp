#pragma once

#include <boost/asio/ssl/context.hpp>

namespace pep {

void TrustSystemRootCAs(boost::asio::ssl::context& ctx);

bool VerifyCertificateBasedOnExpectedCommonName(
  const std::string& expectedCommonName,
  bool preverified,
  boost::asio::ssl::verify_context& verifyCtx);

}
