#include <pep/crypto/CryptoSerializers.hpp>
#include <pep/keyserver/KeyServerSerializers.hpp>

namespace pep {

EnrollmentRequest Serializer<EnrollmentRequest>::fromProtocolBuffer(proto::EnrollmentRequest&& source) const {
  return EnrollmentRequest(
    Serialization::FromProtocolBuffer(std::move(*source.mutable_certificate_signing_request())),
    source.oauth_token());
}

void Serializer<EnrollmentRequest>::moveIntoProtocolBuffer(proto::EnrollmentRequest& dest, EnrollmentRequest value) const {
  *dest.mutable_oauth_token() = std::move(value.mOAuthToken);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_certificate_signing_request(), std::move(value.mCertificateSigningRequest));
}

EnrollmentResponse Serializer<EnrollmentResponse>::fromProtocolBuffer(proto::EnrollmentResponse&& source) const {
  return EnrollmentResponse{
    .mCertificateChain = Serialization::FromProtocolBuffer(std::move(*source.mutable_certificate_chain()))
  };
}

void Serializer<EnrollmentResponse>::moveIntoProtocolBuffer(proto::EnrollmentResponse& dest, EnrollmentResponse value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_certificate_chain(), std::move(value.mCertificateChain));
}

void Serializer<TokenBlockingTokenIdentifier>::moveIntoProtocolBuffer(
    proto::TokenBlockingTokenIdentifier& dest,
    TokenBlockingTokenIdentifier value) const {
  dest.set_subject(value.subject);
  dest.set_usergroup(value.userGroup);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_issuedatetime(), value.issueDateTime);
}

TokenBlockingTokenIdentifier Serializer<TokenBlockingTokenIdentifier>::fromProtocolBuffer(
    proto::TokenBlockingTokenIdentifier&& source) const {
  return {
      .subject = std::move(*source.mutable_subject()),
      .userGroup = std::move(*source.mutable_usergroup()),
      .issueDateTime = Serialization::FromProtocolBuffer(std::move(*source.mutable_issuedatetime()))};
}

void Serializer<TokenBlockingBlocklistEntry>::moveIntoProtocolBuffer(
    proto::TokenBlockingBlocklistEntry& dest,
    TokenBlockingBlocklistEntry value) const {
  dest.set_id(value.id);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_target(), std::move(value.target));
  dest.set_metadatanote(std::move(value.metadata.note));
  dest.set_metadataissuer(std::move(value.metadata.issuer));
  Serialization::MoveIntoProtocolBuffer(
      *dest.mutable_metadatacreationdatetime(),
      value.metadata.creationDateTime);
}

TokenBlockingBlocklistEntry Serializer<TokenBlockingBlocklistEntry>::fromProtocolBuffer(
    proto::TokenBlockingBlocklistEntry&& source) const {
  return {
      .id = source.id(),
      .target = Serialization::FromProtocolBuffer(std::move(*source.mutable_target())),
      .metadata{
          .note = std::move(*source.mutable_metadatanote()),
          .issuer = std::move(*source.mutable_metadataissuer()),
          .creationDateTime =
              Serialization::FromProtocolBuffer(std::move(*source.mutable_metadatacreationdatetime()))}};
}

void Serializer<TokenBlockingListResponse>::moveIntoProtocolBuffer(
    proto::TokenBlockingListResponse& dest,
    pep::TokenBlockingListResponse value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_entries(), std::move(value.entries));
}

TokenBlockingListResponse Serializer<TokenBlockingListResponse>::fromProtocolBuffer(
    proto::TokenBlockingListResponse&& source) const {
  return {.entries = [&] {
    std::vector<TokenBlockingBlocklistEntry> ids;
    Serialization::AssignFromRepeatedProtocolBuffer(ids, std::move(*source.mutable_entries()));
    return ids;
  }()};
}

void Serializer<TokenBlockingCreateRequest>::moveIntoProtocolBuffer(
    proto::TokenBlockingCreateRequest& dest,
    pep::TokenBlockingCreateRequest value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_target(), std::move(value.target));
  dest.set_note(std::move(value.note));
}

TokenBlockingCreateRequest Serializer<TokenBlockingCreateRequest>::fromProtocolBuffer(
    proto::TokenBlockingCreateRequest&& source) const {
  return {
      .target = Serialization::FromProtocolBuffer(std::move(*source.mutable_target())),
      .note = std::move(*source.mutable_note())};
}

void Serializer<TokenBlockingCreateResponse>::moveIntoProtocolBuffer(
    proto::TokenBlockingCreateResponse& dest,
    pep::TokenBlockingCreateResponse value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_entry(), std::move(value.entry));
}

TokenBlockingCreateResponse Serializer<TokenBlockingCreateResponse>::fromProtocolBuffer(
    proto::TokenBlockingCreateResponse&& source) const {
  return {.entry = Serialization::FromProtocolBuffer(std::move(*source.mutable_entry()))};
}

void Serializer<TokenBlockingRemoveRequest>::moveIntoProtocolBuffer(
    proto::TokenBlockingRemoveRequest& dest,
    pep::TokenBlockingRemoveRequest value) const {
  dest.set_id(value.id);
}

TokenBlockingRemoveRequest Serializer<TokenBlockingRemoveRequest>::fromProtocolBuffer(
    proto::TokenBlockingRemoveRequest&& source) const {
  return {.id = source.id()};
}

void Serializer<TokenBlockingRemoveResponse>::moveIntoProtocolBuffer(
    proto::TokenBlockingRemoveResponse& dest,
    pep::TokenBlockingRemoveResponse value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_entry(), std::move(value.entry));
}

TokenBlockingRemoveResponse Serializer<TokenBlockingRemoveResponse>::fromProtocolBuffer(
    proto::TokenBlockingRemoveResponse&& source) const {
  return {.entry = Serialization::FromProtocolBuffer(std::move(*source.mutable_entry()))};
}

} // namespace pep
