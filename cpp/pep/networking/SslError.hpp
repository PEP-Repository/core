#pragma once

#include <boost/system/error_code.hpp> // IWYU pragma: keep (error_code is declared in an internal Boost header)

namespace pep::networking {

/// \brief Determines if a Boost error_code represents a specific (Open)SSL error (code).
/// \param error The Boost error_code to inspect.
/// \param code The OpenSSL error code.
/// \return TRUE if the error_code represents the specified (Open)SSL error (code); FALSE if not.
bool IsSpecificSslError(const boost::system::error_code& error, int code);

}
