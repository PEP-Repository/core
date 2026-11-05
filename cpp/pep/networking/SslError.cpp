#include <pep/networking/SslError.hpp>

#include <boost/asio/ssl/error.hpp> // for boost::asio::error::ssl_category

#include <openssl/err.h>

namespace pep::networking {

bool IsSpecificSslError(const boost::system::error_code& error, int code) {
  return error.category() == boost::asio::error::ssl_category && ERR_GET_REASON(static_cast<unsigned long>(error.value())) == code;
}

}
