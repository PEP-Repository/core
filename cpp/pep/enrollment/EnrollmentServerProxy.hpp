#pragma once

#include <pep/enrollment/KeyComponentMessages.hpp>
#include <pep/server/SigningServerProxy.hpp>

namespace pep {

class EnrollmentServerProxy : public SigningServerProxy {
public:
  using SigningServerProxy::SigningServerProxy;

  rxcpp::observable<KeyComponentResponse> requestKeyComponent(SignedKeyComponentRequest request) const; // Must be pre-signed because caller (who is presumably our MessageSigner) is enrolling
};

}
