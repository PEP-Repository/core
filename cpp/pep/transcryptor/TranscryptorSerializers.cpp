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
    .keys = std::move(keys),
    .clientCertificateChain = Serialization::FromProtocolBuffer(std::move(*source.mutable_client_certificate_chain()))
  };
}

void Serializer<RekeyRequest>::moveIntoProtocolBuffer(proto::RekeyRequest& dest, RekeyRequest value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_client_certificate_chain(), std::move(value.clientCertificateChain));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_keys(), std::move(value.keys));
}

RekeyResponse Serializer<RekeyResponse>::fromProtocolBuffer(proto::RekeyResponse&& source) const {
  RekeyResponse result;
  Serialization::AssignFromRepeatedProtocolBuffer(result.keys, std::move(*source.mutable_keys()));
  return result;
}

void Serializer<RekeyResponse>::moveIntoProtocolBuffer(proto::RekeyResponse& dest, RekeyResponse value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_keys(), std::move(value.keys));
}

void Serializer<TranscryptorRequestEntries>::moveIntoProtocolBuffer(proto::TranscryptorRequestEntries& dest, TranscryptorRequestEntries value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_entries(),
    std::move(value.entries));
}

TranscryptorRequestEntries Serializer<TranscryptorRequestEntries>::fromProtocolBuffer(proto::TranscryptorRequestEntries&& source) const {
  TranscryptorRequestEntries result;
  Serialization::AssignFromRepeatedProtocolBuffer(result.entries,
    std::move(*source.mutable_entries()));
  return result;
}

void Serializer<TranscryptorRequest>::moveIntoProtocolBuffer(proto::TranscryptorRequest& dest, TranscryptorRequest value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_request(),
    std::move(value.request));
}

TranscryptorRequest Serializer<TranscryptorRequest>::fromProtocolBuffer(proto::TranscryptorRequest&& source) const {
  return TranscryptorRequest{
    .request = Serialization::FromProtocolBuffer(std::move(*source.mutable_request())),
  };
}

TranscryptorRequestEntry Serializer<TranscryptorRequestEntry>::fromProtocolBuffer(proto::TranscryptorRequestEntry&& source) const {
  return TranscryptorRequestEntry(
    PolymorphicPseudonym(Serialization::FromProtocolBuffer(std::move(*source.mutable_polymorphic()))),
    EncryptedLocalPseudonym(Serialization::FromProtocolBuffer(std::move(*source.mutable_access_manager()))),
    EncryptedLocalPseudonym(Serialization::FromProtocolBuffer(std::move(*source.mutable_storage_facility()))),
    EncryptedLocalPseudonym(Serialization::FromProtocolBuffer(std::move(*source.mutable_transcryptor()))),
    source.has_user_group() ?
    std::optional<EncryptedLocalPseudonym>(
      //NOLINTNEXTLINE(performance-move-const-arg) False positive
      Serialization::FromProtocolBuffer(std::move(*source.mutable_user_group()))
    ) : std::nullopt,
    Serialization::FromProtocolBuffer(std::move(*source.mutable_access_manager_proof())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_storage_facility_proof())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_transcryptor_proof())),
    source.has_user_group_proof() ?
    std::optional<RskProof>(
      //NOLINTNEXTLINE(performance-move-const-arg) False positive
      Serialization::FromProtocolBuffer(std::move(*source.mutable_user_group_proof()))
    ) : std::nullopt
  );
}

void Serializer<TranscryptorRequestEntry>::moveIntoProtocolBuffer(proto::TranscryptorRequestEntry& dest, TranscryptorRequestEntry value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_polymorphic(), value.polymorphic.getValidElgamalEncryption());
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_access_manager(), value.accessManager.getValidElgamalEncryption());
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_storage_facility(), value.storageFacility.getValidElgamalEncryption());
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_transcryptor(), value.transcryptor.getValidElgamalEncryption());
  if (value.userGroup)
    Serialization::MoveIntoProtocolBuffer(
      *dest.mutable_user_group(),
      value.userGroup->getValidElgamalEncryption()
    );
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_access_manager_proof(), value.accessManagerProof);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_storage_facility_proof(), value.storageFacilityProof);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_transcryptor_proof(), value.transcryptorProof);
  if (value.userGroupProof)
    Serialization::MoveIntoProtocolBuffer(
      *dest.mutable_user_group_proof(),
      *value.userGroupProof
    );
}

void Serializer<TranscryptorResponse>::moveIntoProtocolBuffer(proto::TranscryptorResponse& dest, TranscryptorResponse value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_entries(), std::move(value.entries));
  *dest.mutable_id() = std::move(value.id);
}

TranscryptorResponse Serializer<TranscryptorResponse>::fromProtocolBuffer(proto::TranscryptorResponse&& source) const {
  TranscryptorResponse result;
  Serialization::AssignFromRepeatedProtocolBuffer(result.entries,
    std::move(*source.mutable_entries()));
  result.id = std::move(*source.mutable_id());
  return result;
}

void Serializer<LogIssuedTicketRequest>::moveIntoProtocolBuffer(proto::LogIssuedTicketRequest& dest, LogIssuedTicketRequest value) const {
  Serialization::MoveIntoProtocolBuffer(
    *dest.mutable_ticket(),
    std::move(value.ticket)
  );
  *dest.mutable_id() = std::move(value.id);
}

LogIssuedTicketRequest Serializer<LogIssuedTicketRequest>::fromProtocolBuffer(proto::LogIssuedTicketRequest&& source) const {
  LogIssuedTicketRequest ret;
  ret.id = std::move(*source.mutable_id());
  ret.ticket = Serialization::FromProtocolBuffer(std::move(
    *source.mutable_ticket()));
  return ret;
}

void Serializer<LogIssuedTicketResponse>::moveIntoProtocolBuffer(proto::LogIssuedTicketResponse& dest, LogIssuedTicketResponse value) const {
  Serialization::MoveIntoProtocolBuffer(
    *dest.mutable_signature(),
    std::move(value.signature)
  );
}

LogIssuedTicketResponse Serializer<LogIssuedTicketResponse>::fromProtocolBuffer(proto::LogIssuedTicketResponse&& source) const {
  return LogIssuedTicketResponse(Serialization::FromProtocolBuffer(std::move(*source.mutable_signature())));
}

}
