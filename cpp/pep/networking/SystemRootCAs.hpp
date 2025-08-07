#pragma once

#include <boost/asio/ssl/context.hpp>

namespace pep {

void TrustSystemRootCAs(boost::asio::ssl::context& ctx);

}
