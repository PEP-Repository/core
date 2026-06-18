#include <pep/registrationserver/RegistrationServerSerializers.hpp>

#include <pep/elgamal/ElgamalSerializers.hpp>

namespace pep {

PEPIdRegistrationResponse Serializer<PEPIdRegistrationResponse>::fromProtocolBuffer(proto::PEPIdRegistrationResponse&& source) const {
  PEPIdRegistrationResponse result;
  result.pepId_ = source.pep_id();
  return result;
}

void Serializer<PEPIdRegistrationResponse>::moveIntoProtocolBuffer(proto::PEPIdRegistrationResponse& dest, PEPIdRegistrationResponse value) const {
  dest.set_pep_id(value.pepId_);
}

RegistrationRequest Serializer<RegistrationRequest>::fromProtocolBuffer(proto::RegistrationRequest&& source) const {
  RegistrationRequest result;
  result.polymorphicPseudonym_ = PolymorphicPseudonym(Serialization::FromProtocolBuffer(std::move(*source.mutable_polymorph_pseudonym())));
  result.encryptedIdentifier_ = std::move(*source.mutable_encrypted_identifier());
  result.encryptionPublicKeyPem_ = std::move(*source.mutable_encryption_public_key_pem());
  return result;
}

void Serializer<RegistrationRequest>::moveIntoProtocolBuffer(proto::RegistrationRequest& dest, RegistrationRequest value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_polymorph_pseudonym(), value.polymorphicPseudonym_.getValidElgamalEncryption());
  *dest.mutable_encrypted_identifier() = std::move(value.encryptedIdentifier_);
  *dest.mutable_encryption_public_key_pem() = std::move(value.encryptionPublicKeyPem_);
}

ListCastorImportColumnsRequest Serializer<ListCastorImportColumnsRequest>::fromProtocolBuffer(proto::ListCastorImportColumnsRequest&& source) const {
  ListCastorImportColumnsRequest result;
  result.spColumn_ = std::move(*source.mutable_sp_column());
  result.answerSetCount_ = source.answer_set_count();

  return result;
}

void Serializer<ListCastorImportColumnsRequest>::moveIntoProtocolBuffer(proto::ListCastorImportColumnsRequest& dest, ListCastorImportColumnsRequest value) const {
  *dest.mutable_sp_column() = std::move(value.spColumn_);
  dest.set_answer_set_count(value.answerSetCount_);
}

ListCastorImportColumnsResponse Serializer<ListCastorImportColumnsResponse>::fromProtocolBuffer(proto::ListCastorImportColumnsResponse&& source) const {
  ListCastorImportColumnsResponse result;
  result.importColumns_.reserve(static_cast<size_t>(source.import_columns().size()));
  for (auto& name : *source.mutable_import_columns()) {
    result.importColumns_.push_back(std::move(name));
  }
  return result;
}

void Serializer<ListCastorImportColumnsResponse>::moveIntoProtocolBuffer(proto::ListCastorImportColumnsResponse& dest, ListCastorImportColumnsResponse value) const {
  dest.mutable_import_columns()->Reserve(static_cast<int>(value.importColumns_.size()));
  for (auto& name : value.importColumns_)
    dest.add_import_columns(std::move(name));
}

}
