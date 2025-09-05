#pragma once

#include <optional>
#include <stdexcept>
#include <string>

#include <boost/url/url.hpp>

namespace pep {

class OAuthError : public std::exception {
private:
  std::string mError;
  std::string mDescription;
  std::string mWhat;

public:
  OAuthError(std::string error, std::string description);

  static std::optional<OAuthError> TryRead(const boost::urls::url& source);

  const char* what() const noexcept override;
};

}
