#include <pep/ticketing/TicketingSerializers.hpp>

#include <pep/crypto/CryptoSerializers.hpp>
#include <pep/elgamal/ElgamalSerializers.hpp>

namespace pep {

LocalPseudonyms Serializer<LocalPseudonyms>::fromProtocolBuffer(proto::LocalPseudonyms&& source) const {
  LocalPseudonyms result{
    .mAccessManager = EncryptedLocalPseudonym(Serialization::FromProtocolBuffer(std::move(*source.mutable_access_manager()))),
    .mStorageFacility = EncryptedLocalPseudonym(Serialization::FromProtocolBuffer(std::move(*source.mutable_storage_facility()))),
    .mPolymorphic = PolymorphicPseudonym(Serialization::FromProtocolBuffer(std::move(*source.mutable_polymorphic()))),
  };

  if (source.has_access_group()) {
    result.mAccessGroup = EncryptedLocalPseudonym(Serialization::FromProtocolBuffer(std::move(*source.mutable_access_group())));
  }

  return result;
}

void Serializer<LocalPseudonyms>::moveIntoProtocolBuffer(proto::LocalPseudonyms& dest, LocalPseudonyms value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_access_manager(), value.mAccessManager.getValidElgamalEncryption());
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_storage_facility(), value.mStorageFacility.getValidElgamalEncryption());
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_polymorphic(), value.mPolymorphic.getValidElgamalEncryption());
  if (value.mAccessGroup)
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_access_group(), value.mAccessGroup->getValidElgamalEncryption());
}

Ticket2 Serializer<Ticket2>::fromProtocolBuffer(proto::Ticket2&& source) const {
  Ticket2 result;

  result.mTimestamp = Serialization::FromProtocolBuffer(
    std::move(*source.mutable_timestamp()));
  result.mUserGroup = std::move(*source.mutable_user_group());

  result.mModes.reserve(static_cast<size_t>(source.modes().size()));
  for (auto& x : *source.mutable_modes())
    result.mModes.push_back(std::move(x));
  result.mColumns.reserve(static_cast<size_t>(source.columns().size()));
  for (auto& x : *source.mutable_columns())
    result.mColumns.push_back(std::move(x));

  Serialization::AssignFromRepeatedProtocolBuffer(result.mPseudonyms,
    std::move(*source.mutable_pseudonyms()));

  return result;
}

void Serializer<Ticket2>::moveIntoProtocolBuffer(proto::Ticket2& dest, Ticket2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_timestamp(), value.mTimestamp);
  *dest.mutable_user_group() = std::move(value.mUserGroup);
  dest.mutable_modes()->Reserve(static_cast<int>(value.mModes.size()));
  for (auto& x : value.mModes)
    dest.add_modes(std::move(x));
  dest.mutable_columns()->Reserve(static_cast<int>(value.mColumns.size()));
  for (auto& x : value.mColumns)
    dest.add_columns(std::move(x));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_pseudonyms(), std::move(value.mPseudonyms));
}

SignedTicket2 Serializer<SignedTicket2>::fromProtocolBuffer(proto::SignedTicket2&& source) const {
  std::optional<Signature> sig;
  std::optional<Signature> tsSig;
  if (source.has_signature())
    sig = Serialization::FromProtocolBuffer(std::move(*source.mutable_signature()));
  if (source.has_transcryptor_signature())
    tsSig = Serialization::FromProtocolBuffer(std::move(*source.mutable_transcryptor_signature()));
  return SignedTicket2(
    std::move(sig),
    std::move(tsSig),
    std::move(*source.mutable_data())
  );
}

void Serializer<SignedTicket2>::moveIntoProtocolBuffer(proto::SignedTicket2& dest, SignedTicket2 value) const {
  *dest.mutable_data() = std::move(value.mData);
  if (value.mSignature)
    Serialization::MoveIntoProtocolBuffer(
      *dest.mutable_signature(),
      std::move(*value.mSignature)
    );
  if (value.mTranscryptorSignature)
    Serialization::MoveIntoProtocolBuffer(
      *dest.mutable_transcryptor_signature(),
      std::move(*value.mTranscryptorSignature)
    );
}

TicketRequest2 Serializer<TicketRequest2>::fromProtocolBuffer(proto::TicketRequest2&& source) const {
  TicketRequest2 result;
  result.mModes.reserve(static_cast<size_t>(source.modes().size()));
  for (auto& x : *source.mutable_modes())
    result.mModes.push_back(std::move(x));
  result.mParticipantGroups.reserve(static_cast<size_t>(source.participant_groups().size()));
  for (auto& x : *source.mutable_participant_groups())
    result.mParticipantGroups.push_back(std::move(x));
  result.mColumnGroups.reserve(static_cast<size_t>(source.column_groups().size()));
  for (auto& x : *source.mutable_column_groups())
    result.mColumnGroups.push_back(std::move(x));
  result.mColumns.reserve(static_cast<size_t>(source.columns().size()));
  for (auto& x : *source.mutable_columns())
    result.mColumns.push_back(std::move(x));
  result.mRequestIndexedTicket = source.request_indexed_ticket();
  result.mIncludeUserGroupPseudonyms = source.include_user_group_pseudonyms();

  result.mPolymorphicPseudonyms = RangeToVector(
    *source.mutable_polymorphic_pseudonyms()
    | std::views::transform([](proto::ElgamalEncryption& pp) {
      return PolymorphicPseudonym(Serialization::FromProtocolBuffer(std::move(pp)));
    }));
  return result;
}

void Serializer<TicketRequest2>::moveIntoProtocolBuffer(proto::TicketRequest2& dest, TicketRequest2 value) const {
  dest.mutable_modes()->Reserve(static_cast<int>(value.mModes.size()));
  for (auto& x : value.mModes)
    dest.add_modes(std::move(x));
  dest.mutable_participant_groups()->Reserve(static_cast<int>(value.mParticipantGroups.size()));
  for (auto& x : value.mParticipantGroups)
    dest.add_participant_groups(std::move(x));
  dest.mutable_column_groups()->Reserve(static_cast<int>(value.mColumnGroups.size()));
  for (auto& x : value.mColumnGroups)
    dest.add_column_groups(std::move(x));
  dest.mutable_columns()->Reserve(static_cast<int>(value.mColumns.size()));
  for (auto& x : value.mColumns)
    dest.add_columns(std::move(x));
  dest.set_request_indexed_ticket(value.mRequestIndexedTicket);
  dest.set_include_user_group_pseudonyms(value.mIncludeUserGroupPseudonyms);

  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_polymorphic_pseudonyms(),
    value.mPolymorphicPseudonyms
    | std::views::transform(&PolymorphicPseudonym::getValidElgamalEncryption));
}

SignedTicketRequest2 Serializer<SignedTicketRequest2>::fromProtocolBuffer(proto::SignedTicketRequest2&& source) const {
  std::optional<Signature> sig;
  std::optional<Signature> logSig;
  if (source.has_signature())
    sig = Serialization::FromProtocolBuffer(std::move(*source.mutable_signature()));
  if (source.has_log_signature())
    logSig = Serialization::FromProtocolBuffer(std::move(*source.mutable_log_signature()));
  return SignedTicketRequest2(
    std::move(sig),
    std::move(logSig),
    std::move(*source.mutable_data())
  );
}

void Serializer<SignedTicketRequest2>::moveIntoProtocolBuffer(proto::SignedTicketRequest2& dest, SignedTicketRequest2 value) const {
  *dest.mutable_data() = std::move(value.mData);
  if (value.mSignature)
    Serialization::MoveIntoProtocolBuffer(
      *dest.mutable_signature(),
      std::move(*value.mSignature)
    );
  if (value.mLogSignature)
    Serialization::MoveIntoProtocolBuffer(
      *dest.mutable_log_signature(),
      std::move(*value.mLogSignature)
    );
}

}
