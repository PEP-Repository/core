#pragma once

#include <pep/auth/SigningSerializers.hpp>
#include <pep/authserver/AuthserverMessages.hpp>

#include <Messages.pb.h> // for proto::TokenRequest

namespace pep {

PEP_DEFINE_CODED_SERIALIZER(TokenRequest);
PEP_DEFINE_SIGNED_SERIALIZATION(TokenRequest);
PEP_DEFINE_CODED_SERIALIZER(TokenResponse);

}
