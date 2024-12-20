#pragma once

#include <pep/authserver/AsaMessages.hpp>
#include <pep/crypto/CryptoSerializers.hpp>

namespace pep {

PEP_DEFINE_CODED_SERIALIZER(AsaTokenRequest);
PEP_DEFINE_SIGNED_SERIALIZATION(AsaTokenRequest);
PEP_DEFINE_CODED_SERIALIZER(AsaTokenResponse);
PEP_DEFINE_CODED_SERIALIZER(AsaCreateUser);
PEP_DEFINE_CODED_SERIALIZER(AsaRemoveUser);
PEP_DEFINE_CODED_SERIALIZER(AsaAddUserIdentifier);
PEP_DEFINE_CODED_SERIALIZER(AsaRemoveUserIdentifier);
PEP_DEFINE_CODED_SERIALIZER(UserGroupProperties);
PEP_DEFINE_CODED_SERIALIZER(AsaCreateUserGroup);
PEP_DEFINE_CODED_SERIALIZER(AsaRemoveUserGroup);
PEP_DEFINE_CODED_SERIALIZER(AsaModifyUserGroup);
PEP_DEFINE_CODED_SERIALIZER(AsaAddUserToGroup);
PEP_DEFINE_CODED_SERIALIZER(AsaRemoveUserFromGroup);
PEP_DEFINE_CODED_SERIALIZER(AsaMutationRequest);
PEP_DEFINE_SIGNED_SERIALIZATION(AsaMutationRequest);
PEP_DEFINE_EMPTY_SERIALIZER(AsaMutationResponse);
PEP_DEFINE_CODED_SERIALIZER(AsaQueryResponse);
PEP_DEFINE_CODED_SERIALIZER(AsaQRUserGroup);
PEP_DEFINE_CODED_SERIALIZER(AsaQRUser);
PEP_DEFINE_CODED_SERIALIZER(AsaQuery);
PEP_DEFINE_SIGNED_SERIALIZATION(AsaQuery);

}
