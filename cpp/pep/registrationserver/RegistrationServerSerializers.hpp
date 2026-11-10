#pragma once

#include <pep/auth/SigningSerializers.hpp>
#include <pep/registrationserver/RegistrationServerMessages.hpp>

#include <Messages.pb.h> // for proto::PEPIdRegistrationRequest

namespace pep {

PEP_DEFINE_EMPTY_SERIALIZER(PEPIdRegistrationRequest);
PEP_DEFINE_SIGNED_SERIALIZATION(PEPIdRegistrationRequest);
PEP_DEFINE_CODED_SERIALIZER(PEPIdRegistrationResponse);

PEP_DEFINE_CODED_SERIALIZER(RegistrationRequest);
PEP_DEFINE_SIGNED_SERIALIZATION(RegistrationRequest);
PEP_DEFINE_EMPTY_SERIALIZER(RegistrationResponse);

PEP_DEFINE_CODED_SERIALIZER(ListCastorImportColumnsRequest);
PEP_DEFINE_CODED_SERIALIZER(ListCastorImportColumnsResponse);

}
