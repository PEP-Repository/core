#include <pep/auth/OAuthError.hpp>

namespace pep {

OAuthError::OAuthError(std::string error, std::string description)
  : mError(std::move(error)), description_(std::move(description)), mWhat(description_ + " (" + mError + ")") {
  assert(!mError.empty());
  assert(!description_.empty());
}

std::optional<OAuthError> OAuthError::TryRead(const boost::urls::url& source) {
  const auto& params = source.params();
  if (auto errorIt = params.find("error"); errorIt != params.end()) {
    auto errorDescrIt = params.find("error_description");
    if (errorDescrIt == params.end()) {
      throw std::runtime_error("Incomplete OAuth error data: missing error_description");
    }
    return OAuthError((*errorIt).value, (*errorDescrIt).value);
  }

  return std::nullopt;
}

const char* OAuthError::what() const noexcept {
  return mWhat.c_str();
}

}
