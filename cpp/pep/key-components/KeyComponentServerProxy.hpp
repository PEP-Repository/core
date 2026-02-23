#pragma once

#include <pep/key-components/KeyComponentMessages.hpp>
#include <pep/server/SigningServerProxy.hpp>

namespace pep {

class KeyComponentServerProxy : public SigningServerProxy {
public:
  using SigningServerProxy::SigningServerProxy;

  rxcpp::observable<KeyComponentResponse> requestKeyComponent(SignedKeyComponentRequest request) const; // Must be pre-signed because caller (who is presumably our MessageSigner) is enrolling
};

}
