#include <pep/structure/StructureSerializers.hpp>
#include <pep/serialization/Serialization.hpp>

namespace pep {

void Serializer<std::shared_ptr<CastorStorageDefinition>>::moveIntoProtocolBuffer(proto::CastorStorageDefinition& dest, std::shared_ptr<CastorStorageDefinition> value) const {
  dest.set_study_type(Serialization::ToProtocolBuffer(value->getStudyType()));
  *dest.mutable_data_column() = value->getDataColumn();
  *dest.mutable_import_study_slug() = value->getImportStudySlug();
  dest.set_immediate_partial_data(value->immediatePartialData());
  *dest.mutable_week_offset_device_column() = value->getWeekOffsetDeviceColumn();
}

std::shared_ptr<CastorStorageDefinition> Serializer<std::shared_ptr<CastorStorageDefinition>>::fromProtocolBuffer(proto::CastorStorageDefinition&& source) const {
  return std::make_shared<CastorStorageDefinition>(
    Serialization::FromProtocolBuffer(source.study_type()),
    std::move(*source.mutable_data_column()), std::move(*source.mutable_import_study_slug()), source.immediate_partial_data(), std::move(*source.mutable_week_offset_device_column()));
}

void Serializer<CastorShortPseudonymDefinition>::moveIntoProtocolBuffer(proto::CastorShortPseudonymDefinition& dest, CastorShortPseudonymDefinition value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_storage(), value.getStorageDefinitions());
  *dest.mutable_study_slug() = value.getStudySlug();
  *dest.mutable_site_abbreviation() = value.getSiteAbbreviation();
}

CastorShortPseudonymDefinition Serializer<CastorShortPseudonymDefinition>::fromProtocolBuffer(proto::CastorShortPseudonymDefinition&& source) const {
  std::vector<std::shared_ptr<CastorStorageDefinition>> storage;
  Serialization::AssignFromRepeatedProtocolBuffer(storage,
    std::move(*source.mutable_storage()));
  return CastorShortPseudonymDefinition(
    std::move(*source.mutable_study_slug()),
    std::move(*source.mutable_site_abbreviation()),
    std::move(storage));
}

void Serializer<ShortPseudonymDefinition>::moveIntoProtocolBuffer(proto::ShortPseudonymDefinition& dest, ShortPseudonymDefinition value) const {
  auto castor = value.getCastor();
  if (castor)
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_castor(), *castor);
  *dest.mutable_column() = value.getColumn().getFullName();
  *dest.mutable_prefix() = value.getPrefix();
  *dest.mutable_description() = value.getConfiguredDescription();
  dest.set_length(value.getLength());
  dest.set_stickers(value.getStickers());
  dest.set_suppress_additional_stickers(value.getSuppressAdditionalStickers()),
    * dest.mutable_study_context() = value.getStudyContext();
}

ShortPseudonymDefinition Serializer<ShortPseudonymDefinition>::fromProtocolBuffer(proto::ShortPseudonymDefinition&& source) const {
  std::optional<CastorShortPseudonymDefinition> castor;
  if (source.has_castor())
    castor = Serialization::FromProtocolBuffer(std::move(*source.mutable_castor()));
  return ShortPseudonymDefinition(
    std::move(*source.mutable_column()),
    std::move(*source.mutable_prefix()),
    source.length(),
    std::move(castor),
    source.stickers(),
    source.suppress_additional_stickers(),
    std::move(*source.mutable_description()),
    std::move(*source.mutable_study_context()));
}

void Serializer<UserPseudonymFormat>::moveIntoProtocolBuffer(proto::UserPseudonymFormat& dest, UserPseudonymFormat value) const {
  *dest.mutable_prefix() = value.getPrefix();
  assert(value.getLength() <= std::numeric_limits<google::protobuf::uint32>::max());
  dest.set_length(static_cast<google::protobuf::uint32>(value.getLength()));
}

UserPseudonymFormat Serializer<UserPseudonymFormat>::fromProtocolBuffer(proto::UserPseudonymFormat&& source) const {
  return UserPseudonymFormat(
    *source.mutable_prefix(),
    source.length()
  );
}


void Serializer<AdditionalStickerDefinition>::moveIntoProtocolBuffer(proto::AdditionalStickerDefinition& dest, AdditionalStickerDefinition value) const {
  *dest.mutable_column() = std::move(value.mColumn);
  dest.set_visit(value.mVisit);
  dest.set_stickers(value.mStickers);
  dest.set_suppress_additional_stickers(value.mSuppressAdditionalStickers);
  dest.set_study_context(std::move(value.mStudyContext));
}

AdditionalStickerDefinition Serializer<AdditionalStickerDefinition>::fromProtocolBuffer(proto::AdditionalStickerDefinition&& source) const {
  AdditionalStickerDefinition result;
  result.mVisit = source.visit();
  result.mColumn = std::move(*source.mutable_column());
  result.mStickers = source.stickers();
  result.mSuppressAdditionalStickers = source.suppress_additional_stickers();
  result.mStudyContext = std::move(*source.mutable_study_context());
  return result;
}

void Serializer<DeviceRegistrationDefinition>::moveIntoProtocolBuffer(proto::DeviceRegistrationDefinition& dest, DeviceRegistrationDefinition value) const {
  *dest.mutable_column() = std::move(value.columnName);
  *dest.mutable_serial_number_format() = std::move(value.serialNumberFormat);
  *dest.mutable_description() = std::move(value.description);
  *dest.mutable_tooltip() = std::move(value.tooltip);
  *dest.mutable_placeholder() = std::move(value.placeholder);
  *dest.mutable_study_context() = std::move(value.studyContext);
}

DeviceRegistrationDefinition Serializer<DeviceRegistrationDefinition>::fromProtocolBuffer(proto::DeviceRegistrationDefinition&& source) const {
  DeviceRegistrationDefinition result;
  result.columnName = source.column();
  result.serialNumberFormat = source.serial_number_format();
  result.description = source.description();
  result.tooltip = source.tooltip();
  result.placeholder = source.placeholder();
  result.studyContext = source.study_context();
  return result;
}

void Serializer<PseudonymFormat>::moveIntoProtocolBuffer(proto::PseudonymFormat& dest, PseudonymFormat value) const {
  if (value.isGenerable()) {
    proto::GenerablePseudonymFormat generable;
    generable.set_prefix(value.getPrefix());
    assert(value.getNumberOfGeneratedDigits() <= std::numeric_limits<google::protobuf::uint32>::max());
    generable.set_digits(static_cast<google::protobuf::uint32>(value.getNumberOfGeneratedDigits()));

    *dest.mutable_generable() = generable;
  }
  else {
    proto::RegexPseudonymFormat regex;
    regex.set_pattern(value.getRegexPattern());

    *dest.mutable_regex() = regex;
  }
}

PseudonymFormat Serializer<PseudonymFormat>::fromProtocolBuffer(proto::PseudonymFormat&& source) const {
  if (source.has_generable()) {
    return PseudonymFormat(std::move(*source.mutable_generable()->mutable_prefix()), source.generable().digits());
  }
  if (!source.has_regex()) {
    throw std::invalid_argument("Expected generable or regex");
  }
  return PseudonymFormat(std::move(*source.mutable_regex()->mutable_pattern()));
}

void Serializer<AssessorDefinition>::moveIntoProtocolBuffer(proto::AssessorDefinition& dest, AssessorDefinition value) const {
  dest.set_id(value.id);
  dest.set_name(std::move(value.name));
  dest.mutable_study_contexts()->Reserve(static_cast<int>(value.studyContexts.size()));
  for (auto& x : value.studyContexts)
    dest.add_study_contexts(std::move(x));
}

AssessorDefinition Serializer<AssessorDefinition>::fromProtocolBuffer(proto::AssessorDefinition&& source) const {
  AssessorDefinition result;
  result.id = source.id();
  result.name = std::move(*source.mutable_name());
  result.studyContexts.reserve(static_cast<size_t>(source.study_contexts().size()));
  for (auto& x : *source.mutable_study_contexts())
    result.studyContexts.push_back(std::move(x));
  return result;
}

void Serializer<StudyContext>::moveIntoProtocolBuffer(proto::StudyContext& dest, StudyContext value) const {
  dest.set_id(value.getId());
}

StudyContext Serializer<StudyContext>::fromProtocolBuffer(proto::StudyContext&& source) const {
  return StudyContext(source.id());
}

void Serializer<ColumnSpecification>::moveIntoProtocolBuffer(proto::ColumnSpecification& dest, ColumnSpecification value) const {
  dest.set_column(value.getColumn());
  dest.set_requires_directory(value.getRequiresDirectory());

  if (auto associatedShortPseudonymColumn = value.getAssociatedShortPseudonymColumn()) {
    *dest.mutable_associated_short_pseudonym_column() = std::move(*associatedShortPseudonymColumn);
  }
}

ColumnSpecification Serializer<ColumnSpecification>::fromProtocolBuffer(proto::ColumnSpecification&& source) const {
  return ColumnSpecification(
    source.column(),
    source.associated_short_pseudonym_column().empty() ? std::nullopt : std::make_optional(source.associated_short_pseudonym_column()),
    source.requires_directory()
  );
}

void Serializer<ShortPseudonymErratum>::moveIntoProtocolBuffer(proto::ShortPseudonymErratum& dest, ShortPseudonymErratum value) const {
  *dest.mutable_value() = std::move(value.value);
  *dest.mutable_column() = std::move(value.column);
}

ShortPseudonymErratum Serializer<ShortPseudonymErratum>::fromProtocolBuffer(proto::ShortPseudonymErratum&& source) const {
  return ShortPseudonymErratum{ std::move(*source.mutable_value()), std::move(*source.mutable_column()) };
}

void Serializer<GlobalConfiguration>::moveIntoProtocolBuffer(proto::GlobalConfiguration& dest, GlobalConfiguration value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_short_pseudonyms(), value.getShortPseudonyms());
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_additional_stickers(), value.getAdditionalStickers());
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_participant_identifier_formats(), value.getParticipantIdentifierFormats());
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_devices(), value.getDevices());
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_column_specifications(), value.getColumnSpecifications());
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_user_pseudonym_format(), value.getUserPseudonymFormat());
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_assessors(), value.getAssessors());
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_study_contexts(), value.getStudyContexts().getItems());
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_short_pseudonym_errata(), value.getShortPseudonymErrata());
}

GlobalConfiguration Serializer<GlobalConfiguration>::fromProtocolBuffer(proto::GlobalConfiguration&& source) const {
  std::vector<ShortPseudonymDefinition> shortPseudonyms;
  std::vector<AdditionalStickerDefinition> additionalStickers;
  std::vector<PseudonymFormat> participantIdentifierFormats;
  std::vector<DeviceRegistrationDefinition> devices;
  std::vector<AssessorDefinition> assessors;
  std::vector<StudyContext> studyContexts;
  std::vector<ColumnSpecification> columnSpecifications;
  std::vector<ShortPseudonymErratum> spErrata;

  UserPseudonymFormat userPseudonymFormat(Serialization::FromProtocolBuffer(std::move(*source.mutable_user_pseudonym_format())));

  Serialization::AssignFromRepeatedProtocolBuffer(shortPseudonyms,
    std::move(*source.mutable_short_pseudonyms()));
  Serialization::AssignFromRepeatedProtocolBuffer(additionalStickers,
    std::move(*source.mutable_additional_stickers()));
  Serialization::AssignFromRepeatedProtocolBuffer(participantIdentifierFormats,
    std::move(*source.mutable_participant_identifier_formats()));
  Serialization::AssignFromRepeatedProtocolBuffer(devices,
    std::move(*source.mutable_devices()));
  Serialization::AssignFromRepeatedProtocolBuffer(assessors,
    std::move(*source.mutable_assessors()));
  Serialization::AssignFromRepeatedProtocolBuffer(studyContexts,
    std::move(*source.mutable_study_contexts()));
  Serialization::AssignFromRepeatedProtocolBuffer(columnSpecifications,
    std::move(*source.mutable_column_specifications()));
  Serialization::AssignFromRepeatedProtocolBuffer(spErrata,
    std::move(*source.mutable_short_pseudonym_errata()));

  return GlobalConfiguration(
    std::move(participantIdentifierFormats),
    std::move(studyContexts),
    std::move(shortPseudonyms),
    std::move(userPseudonymFormat),
    std::move(additionalStickers),
    std::move(devices),
    std::move(assessors),
    std::move(columnSpecifications),
    std::move(spErrata));
}

}
