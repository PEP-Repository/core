#pragma once

#include <pep/auth/SigningSerializers.hpp>
#include <pep/authserver/AuthserverMessages.hpp>

namespace pep {

PEP_DEFINE_CODED_SERIALIZER(TokenRequest);
PEP_DEFINE_SIGNED_SERIALIZATION(TokenRequest);
PEP_DEFINE_CODED_SERIALIZER(TokenResponse);

}
