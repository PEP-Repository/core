#pragma once

#include <pep/auth/SigningSerializers.hpp>
#include <pep/keyserver/KeyServerMessages.hpp>

namespace pep {

PEP_DEFINE_CODED_SERIALIZER(EnrollmentRequest);
PEP_DEFINE_CODED_SERIALIZER(EnrollmentResponse);

// Our serialization mechanism only supports types that exist directly in the "pep" namespace, so
// we introduce aliases in that scope for types from sub-namespaces that need to be (de)serialized.
using TokenBlockingTokenIdentifier = tokenBlocking::TokenIdentifier;
using TokenBlockingBlocklistEntry = tokenBlocking::BlocklistEntry;

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
