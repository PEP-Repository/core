#include <pep/utils/OpensslUtils.hpp>
#include <openssl/err.h>
#include <pep/utils/Defer.hpp>

namespace pep {

std::string TakeOpenSSLErrors() {
  PEP_DEFER(ERR_clear_error());
  std::string prefix = " OpenSSL Error: ";
  BIO* bio = BIO_new(BIO_s_mem());
  if (!bio) {
    throw std::runtime_error("Failed to create IO buffer (BIO) in TakeOpenSSLErrors().");
  }
  PEP_DEFER(BIO_free(bio));

  ERR_print_errors(bio);
  return prefix + OpenSSLBIOToString(bio);
}

OpenSSLError::OpenSSLError(const std::string& message)
    : std::runtime_error(message + " " + TakeOpenSSLErrors()) {}

std::string OpenSSLBIOToString(BIO* bio) {
  char* data{};
  auto len = BIO_get_mem_data(bio, &data);
  if (len <= 0) {
    throw pep::OpenSSLError("Failed to get data from BIO in OpenSSLBIOToString");
  }
  return std::string(data, static_cast<size_t>(len));
}

} // namespace pep
