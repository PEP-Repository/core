#include <pep/registrationserver/RegistrationServerSerializers.hpp>

#include <pep/elgamal/ElgamalSerializers.hpp>

namespace pep {

PEPIdRegistrationResponse Serializer<PEPIdRegistrationResponse>::fromProtocolBuffer(proto::PEPIdRegistrationResponse&& source) const {
  PEPIdRegistrationResponse result;
  result.mPepId = source.pep_id();
  return result;
}

void Serializer<PEPIdRegistrationResponse>::moveIntoProtocolBuffer(proto::PEPIdRegistrationResponse& dest, PEPIdRegistrationResponse value) const {
  dest.set_pep_id(value.mPepId);
}

RegistrationRequest Serializer<RegistrationRequest>::fromProtocolBuffer(proto::RegistrationRequest&& source) const {
  RegistrationRequest result;
  result.mPolymorphicPseudonym = PolymorphicPseudonym(Serialization::FromProtocolBuffer(std::move(*source.mutable_polymorph_pseudonym())));
  result.mEncryptedIdentifier = std::move(*source.mutable_encrypted_identifier());
  result.mEncryptionPublicKeyPem = std::move(*source.mutable_encryption_public_key_pem());
  return result;
}

void Serializer<RegistrationRequest>::moveIntoProtocolBuffer(proto::RegistrationRequest& dest, RegistrationRequest value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_polymorph_pseudonym(), value.mPolymorphicPseudonym.getValidElgamalEncryption());
  *dest.mutable_encrypted_identifier() = std::move(value.mEncryptedIdentifier);
  *dest.mutable_encryption_public_key_pem() = std::move(value.mEncryptionPublicKeyPem);
}

ListCastorImportColumnsRequest Serializer<ListCastorImportColumnsRequest>::fromProtocolBuffer(proto::ListCastorImportColumnsRequest&& source) const {
  ListCastorImportColumnsRequest result;
  result.mSpColumn = std::move(*source.mutable_sp_column());
  result.mAnswerSetCount = source.answer_set_count();

  return result;
}

void Serializer<ListCastorImportColumnsRequest>::moveIntoProtocolBuffer(proto::ListCastorImportColumnsRequest& dest, ListCastorImportColumnsRequest value) const {
  *dest.mutable_sp_column() = std::move(value.mSpColumn);
  dest.set_answer_set_count(value.mAnswerSetCount);
}

ListCastorImportColumnsResponse Serializer<ListCastorImportColumnsResponse>::fromProtocolBuffer(proto::ListCastorImportColumnsResponse&& source) const {
  ListCastorImportColumnsResponse result;
  result.mImportColumns.reserve(static_cast<size_t>(source.import_columns().size()));
  for (auto& name : *source.mutable_import_columns()) {
    result.mImportColumns.push_back(std::move(name));
  }
  return result;
}

void Serializer<ListCastorImportColumnsResponse>::moveIntoProtocolBuffer(proto::ListCastorImportColumnsResponse& dest, ListCastorImportColumnsResponse value) const {
  dest.mutable_import_columns()->Reserve(static_cast<int>(value.mImportColumns.size()));
  for (auto& name : value.mImportColumns)
    dest.add_import_columns(std::move(name));
}

}
