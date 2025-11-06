#pragma once

#include <pep/server/SigningServerProxy.hpp>

namespace pep {

class AuthServerProxy : public SigningServerProxy {
public:
  using SigningServerProxy::SigningServerProxy;

  rxcpp::observable<std::string> requestToken(std::string subject, std::string group, Timestamp expirationTime) const;
};

}
