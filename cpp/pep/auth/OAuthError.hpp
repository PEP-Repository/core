#pragma once

#include <optional>
#include <stdexcept>
#include <string>

#include <boost/url/url.hpp>

namespace pep {

class OAuthError : public std::exception {
private:
  std::string error_;
  std::string description_;
  std::string what_;

public:
  OAuthError(std::string error, std::string description);

  static std::optional<OAuthError> TryRead(const boost::urls::url& source);

  const char* what() const noexcept override;
};

}
