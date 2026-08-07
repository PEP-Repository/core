#include <pep/ticketing/TicketingSerializers.hpp>

#include <pep/auth/SigningSerializers.hpp>
#include <pep/serialization/TimestampSerializer.hpp>
#include <pep/elgamal/ElgamalSerializers.hpp>

#include <ranges>

namespace pep {

using namespace std::ranges;

LocalPseudonyms Serializer<LocalPseudonyms>::fromProtocolBuffer(proto::LocalPseudonyms&& source) const {
  LocalPseudonyms result{
    .accessManager = EncryptedLocalPseudonym(Serialization::FromProtocolBuffer(std::move(*source.mutable_access_manager()))),
    .storageFacility = EncryptedLocalPseudonym(Serialization::FromProtocolBuffer(std::move(*source.mutable_storage_facility()))),
    .polymorphic = PolymorphicPseudonym(Serialization::FromProtocolBuffer(std::move(*source.mutable_polymorphic()))),
  };

  if (source.has_access_group()) {
    result.accessGroup = EncryptedLocalPseudonym(Serialization::FromProtocolBuffer(std::move(*source.mutable_access_group())));
  }

  return result;
}

void Serializer<LocalPseudonyms>::moveIntoProtocolBuffer(proto::LocalPseudonyms& dest, LocalPseudonyms value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_access_manager(), value.accessManager.getValidElgamalEncryption());
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_storage_facility(), value.storageFacility.getValidElgamalEncryption());
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_polymorphic(), value.polymorphic.getValidElgamalEncryption());
  if (value.accessGroup)
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_access_group(), value.accessGroup->getValidElgamalEncryption());
}

Ticket2 Serializer<Ticket2>::fromProtocolBuffer(proto::Ticket2&& source) const {
  Ticket2 result;

  result.timestamp = Serialization::FromProtocolBuffer(
    std::move(*source.mutable_timestamp()));
  result.userGroup = std::move(*source.mutable_user_group());

  result.modes = *source.mutable_modes() | views::as_rvalue | to<std::vector>();
  result.columns = *source.mutable_columns() | views::as_rvalue | to<std::vector>();

  Serialization::AssignFromRepeatedProtocolBuffer(result.accessSubjects,
    std::move(*source.mutable_access_subjects()));

  return result;
}

void Serializer<Ticket2>::moveIntoProtocolBuffer(proto::Ticket2& dest, Ticket2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_timestamp(), value.timestamp);
  *dest.mutable_user_group() = std::move(value.userGroup);
  dest.mutable_modes()->Reserve(static_cast<int>(value.modes.size()));
  for (auto& x : value.modes)
    dest.add_modes(std::move(x));
  dest.mutable_columns()->Reserve(static_cast<int>(value.columns.size()));
  for (auto& x : value.columns)
    dest.add_columns(std::move(x));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_access_subjects(), std::move(value.accessSubjects));
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
  *dest.mutable_data() = std::move(value.data_);
  if (value.signature_)
    Serialization::MoveIntoProtocolBuffer(
      *dest.mutable_signature(),
      std::move(*value.signature_)
    );
  if (value.transcryptorSignature_)
    Serialization::MoveIntoProtocolBuffer(
      *dest.mutable_transcryptor_signature(),
      std::move(*value.transcryptorSignature_)
    );
}

TicketRequest2 Serializer<TicketRequest2>::fromProtocolBuffer(proto::TicketRequest2&& source) const {
  TicketRequest2 result;
  result.modes = *source.mutable_modes() | views::as_rvalue | to<std::vector>();
  result.participantGroups = *source.mutable_participant_groups() | views::as_rvalue | to<std::vector>();
  result.columnGroups = *source.mutable_column_groups() | views::as_rvalue | to<std::vector>();
  result.columns = *source.mutable_columns() | views::as_rvalue | to<std::vector>();
  result.requestIndexedTicket = source.request_indexed_ticket();
  result.includeUserGroupPseudonyms = source.include_user_group_pseudonyms();

  const auto transformToPolymorphicPseudonym = views::transform([](proto::ElgamalEncryption& pp) {
    return PolymorphicPseudonym(Serialization::FromProtocolBuffer(std::move(pp)));
  });
  result.accessSubjects =
    *source.mutable_access_subjects() | transformToPolymorphicPseudonym | to<std::vector>();
  return result;
}

void Serializer<TicketRequest2>::moveIntoProtocolBuffer(proto::TicketRequest2& dest, TicketRequest2 value) const {
  dest.mutable_modes()->Reserve(static_cast<int>(value.modes.size()));
  for (auto& x : value.modes)
    dest.add_modes(std::move(x));
  dest.mutable_participant_groups()->Reserve(static_cast<int>(value.participantGroups.size()));
  for (auto& x : value.participantGroups)
    dest.add_participant_groups(std::move(x));
  dest.mutable_column_groups()->Reserve(static_cast<int>(value.columnGroups.size()));
  for (auto& x : value.columnGroups)
    dest.add_column_groups(std::move(x));
  dest.mutable_columns()->Reserve(static_cast<int>(value.columns.size()));
  for (auto& x : value.columns)
    dest.add_columns(std::move(x));
  dest.set_request_indexed_ticket(value.requestIndexedTicket);
  dest.set_include_user_group_pseudonyms(value.includeUserGroupPseudonyms);

  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_access_subjects(),
    value.accessSubjects
    | views::transform(&PolymorphicPseudonym::getValidElgamalEncryption));
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
  *dest.mutable_data() = std::move(value.data_);
  if (value.signature_)
    Serialization::MoveIntoProtocolBuffer(
      *dest.mutable_signature(),
      std::move(*value.signature_)
    );
  if (value.logSignature_)
    Serialization::MoveIntoProtocolBuffer(
      *dest.mutable_log_signature(),
      std::move(*value.logSignature_)
    );
}

}
