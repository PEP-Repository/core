#pragma once

#include <Messages.pb.h>
#include <pep/crypto/CryptoSerializers.hpp>
#include <pep/keyserver/KeyServerMessages.hpp>

namespace pep {

PEP_DEFINE_CODED_SERIALIZER(EnrollmentRequest);
PEP_DEFINE_CODED_SERIALIZER(EnrollmentResponse);

PEP_DEFINE_CODED_SERIALIZER(TokenBlockingTokenIdentifier);
PEP_DEFINE_CODED_SERIALIZER(TokenBlockingBlocklistEntry);
PEP_DEFINE_EMPTY_SERIALIZER(TokenBlockingListRequest);
PEP_DEFINE_SIGNED_SERIALIZATION(TokenBlockingListRequest);
PEP_DEFINE_CODED_SERIALIZER(TokenBlockingListResponse);
PEP_DEFINE_CODED_SERIALIZER(TokenBlockingCreateRequest);
PEP_DEFINE_SIGNED_SERIALIZATION(TokenBlockingCreateRequest);
PEP_DEFINE_CODED_SERIALIZER(TokenBlockingCreateResponse);
PEP_DEFINE_CODED_SERIALIZER(TokenBlockingRemoveRequest);
PEP_DEFINE_SIGNED_SERIALIZATION(TokenBlockingRemoveRequest);
PEP_DEFINE_CODED_SERIALIZER(TokenBlockingRemoveResponse);

} // namespace pep