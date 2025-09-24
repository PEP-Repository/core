#pragma once

#include <pep/transcryptor/TranscryptorMessages.hpp>
#include <pep/serialization/ProtocolBufferedSerializer.hpp>
#include <Messages.pb.h>

namespace pep {

PEP_DEFINE_CODED_SERIALIZER(RekeyRequest);
PEP_DEFINE_CODED_SERIALIZER(RekeyResponse);

PEP_DEFINE_CODED_SERIALIZER(TranscryptorRequest);
PEP_DEFINE_CODED_SERIALIZER(TranscryptorRequestEntry);
PEP_DEFINE_CODED_SERIALIZER(TranscryptorRequestEntries);
PEP_DEFINE_CODED_SERIALIZER(TranscryptorResponse);

PEP_DEFINE_CODED_SERIALIZER(LogIssuedTicketRequest);
PEP_DEFINE_CODED_SERIALIZER(LogIssuedTicketResponse);

}
