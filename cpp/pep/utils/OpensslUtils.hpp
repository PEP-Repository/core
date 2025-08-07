#pragma once

#include <stdexcept>
#include <string>

#include <openssl/bio.h>

namespace pep {

/**
 * @brief Retrieve the OpenSSL error string and clear the OpenSSL error queue.
 * @return A string containing all the OpenSSL error messages.
 */
std::string TakeOpenSSLErrors();

std::string OpenSSLBIOToString(BIO* bio);

/**
 * @brief Exception class for OpenSSL errors.
 */
class OpenSSLError : public std::runtime_error {
public:
    OpenSSLError(const std::string& message);
};

} // namespace pep