#include <pep/transcryptor/TranscryptorSerializers.hpp>
#include <pep/auth/SigningSerializers.hpp>
#include <pep/crypto/CryptoSerializers.hpp>
#include <pep/elgamal/ElgamalSerializers.hpp>
#include <pep/rsk/RskSerializers.hpp>
#include <pep/morphing/MorphingSerializers.hpp>
#include <pep/ticketing/TicketingSerializers.hpp>

namespace pep {

RekeyRequest Serializer<RekeyRequest>::fromProtocolBuffer(proto::RekeyRequest&& source) const {
  std::vector<EncryptedKey> keys;
  Serialization::AssignFromRepeatedProtocolBuffer(keys, std::move(*source.mutable_keys()));
  return RekeyRequest{
    .mKeys = std::move(keys),
    .mClientCertificateChain = Serialization::FromProtocolBuffer(std::move(*source.mutable_client_certificate_chain()))
  };
}

void Serializer<RekeyRequest>::moveIntoProtocolBuffer(proto::RekeyRequest& dest, RekeyRequest value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_client_certificate_chain(), std::move(value.mClientCertificateChain));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_keys(), std::move(value.mKeys));
}

RekeyResponse Serializer<RekeyResponse>::fromProtocolBuffer(proto::RekeyResponse&& source) const {
  RekeyResponse result;
  Serialization::AssignFromRepeatedProtocolBuffer(result.mKeys, std::move(*source.mutable_keys()));
  return result;
}

void Serializer<RekeyResponse>::moveIntoProtocolBuffer(proto::RekeyResponse& dest, RekeyResponse value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_keys(), std::move(value.mKeys));
}

void Serializer<TranscryptorRequestEntries>::moveIntoProtocolBuffer(proto::TranscryptorRequestEntries& dest, TranscryptorRequestEntries value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_entries(),
    std::move(value.mEntries));
}

TranscryptorRequestEntries Serializer<TranscryptorRequestEntries>::fromProtocolBuffer(proto::TranscryptorRequestEntries&& source) const {
  TranscryptorRequestEntries result;
  Serialization::AssignFromRepeatedProtocolBuffer(result.mEntries,
    std::move(*source.mutable_entries()));
  return result;
}

void Serializer<TranscryptorRequest>::moveIntoProtocolBuffer(proto::TranscryptorRequest& dest, TranscryptorRequest value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_request(),
    std::move(value.mRequest));
}

TranscryptorRequest Serializer<TranscryptorRequest>::fromProtocolBuffer(proto::TranscryptorRequest&& source) const {
  TranscryptorRequest result;
  result.mRequest = Serialization::FromProtocolBuffer(
    std::move(*source.mutable_request()));
  return result;
}

TranscryptorRequestEntry Serializer<TranscryptorRequestEntry>::fromProtocolBuffer(proto::TranscryptorRequestEntry&& source) const {
  return TranscryptorRequestEntry(
    PolymorphicPseudonym(Serialization::FromProtocolBuffer(std::move(*source.mutable_polymorphic()))),
    EncryptedLocalPseudonym(Serialization::FromProtocolBuffer(std::move(*source.mutable_access_manager()))),
    EncryptedLocalPseudonym(Serialization::FromProtocolBuffer(std::move(*source.mutable_storage_facility()))),
    EncryptedLocalPseudonym(Serialization::FromProtocolBuffer(std::move(*source.mutable_transcryptor()))),
    source.has_user_group() ?
    std::optional<EncryptedLocalPseudonym>(
      Serialization::FromProtocolBuffer(std::move(*source.mutable_user_group()))
    ) : std::nullopt,
    Serialization::FromProtocolBuffer(std::move(*source.mutable_access_manager_proof())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_storage_facility_proof())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_transcryptor_proof())),
    source.has_user_group_proof() ?
    std::optional<RSKProof>(
      Serialization::FromProtocolBuffer(std::move(*source.mutable_user_group_proof()))
    ) : std::nullopt
  );
}

void Serializer<TranscryptorRequestEntry>::moveIntoProtocolBuffer(proto::TranscryptorRequestEntry& dest, TranscryptorRequestEntry value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_polymorphic(), value.mPolymorphic.getValidElgamalEncryption());
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_access_manager(), value.mAccessManager.getValidElgamalEncryption());
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_storage_facility(), value.mStorageFacility.getValidElgamalEncryption());
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_transcryptor(), value.mTranscryptor.getValidElgamalEncryption());
  if (value.mUserGroup)
    Serialization::MoveIntoProtocolBuffer(
      *dest.mutable_user_group(),
      value.mUserGroup->getValidElgamalEncryption()
    );
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_access_manager_proof(), value.mAccessManagerProof);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_storage_facility_proof(), value.mStorageFacilityProof);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_transcryptor_proof(), value.mTranscryptorProof);
  if (value.mUserGroupProof)
    Serialization::MoveIntoProtocolBuffer(
      *dest.mutable_user_group_proof(),
      *value.mUserGroupProof
    );
}

void Serializer<TranscryptorResponse>::moveIntoProtocolBuffer(proto::TranscryptorResponse& dest, TranscryptorResponse value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_entries(), std::move(value.mEntries));
  *dest.mutable_id() = std::move(value.mId);
}

TranscryptorResponse Serializer<TranscryptorResponse>::fromProtocolBuffer(proto::TranscryptorResponse&& source) const {
  TranscryptorResponse result;
  Serialization::AssignFromRepeatedProtocolBuffer(result.mEntries,
    std::move(*source.mutable_entries()));
  result.mId = std::move(*source.mutable_id());
  return result;
}

void Serializer<LogIssuedTicketRequest>::moveIntoProtocolBuffer(proto::LogIssuedTicketRequest& dest, LogIssuedTicketRequest value) const {
  Serialization::MoveIntoProtocolBuffer(
    *dest.mutable_ticket(),
    std::move(value.mTicket)
  );
  *dest.mutable_id() = std::move(value.mId);
}

LogIssuedTicketRequest Serializer<LogIssuedTicketRequest>::fromProtocolBuffer(proto::LogIssuedTicketRequest&& source) const {
  LogIssuedTicketRequest ret;
  ret.mId = std::move(*source.mutable_id());
  ret.mTicket = Serialization::FromProtocolBuffer(std::move(
    *source.mutable_ticket()));
  return ret;
}

void Serializer<LogIssuedTicketResponse>::moveIntoProtocolBuffer(proto::LogIssuedTicketResponse& dest, LogIssuedTicketResponse value) const {
  Serialization::MoveIntoProtocolBuffer(
    *dest.mutable_signature(),
    std::move(value.mSignature)
  );
}

LogIssuedTicketResponse Serializer<LogIssuedTicketResponse>::fromProtocolBuffer(proto::LogIssuedTicketResponse&& source) const {
  return LogIssuedTicketResponse(Serialization::FromProtocolBuffer(std::move(*source.mutable_signature())));
}

}
