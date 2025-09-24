#include <pep/messaging/MessagingSerializers.hpp>
#include <boost/lexical_cast.hpp>

namespace pep {

ConfigVersion Serializer<ConfigVersion>::fromProtocolBuffer(proto::ConfigVersion&& source) const {
  return ConfigVersion(
    std::move(*source.mutable_project_path()),
    std::move(*source.mutable_reference()),
    std::move(*source.mutable_revision()),
    boost::lexical_cast<unsigned int>(source.major_version()),
    boost::lexical_cast<unsigned int>(source.minor_version()),
    boost::lexical_cast<unsigned int>(source.pipeline_id()),
    boost::lexical_cast<unsigned int>(source.job_id()),
    std::move(*source.mutable_project_caption()));
}

void Serializer<ConfigVersion>::moveIntoProtocolBuffer(proto::ConfigVersion& dest, ConfigVersion value) const {
  *dest.mutable_project_path() = value.getProjectPath();
  *dest.mutable_reference() = value.getReference();
  *dest.mutable_revision() = value.getRevision();
  *dest.mutable_major_version() = std::to_string(value.getSemver().getMajorVersion());
  *dest.mutable_minor_version() = std::to_string(value.getSemver().getMinorVersion());
  *dest.mutable_pipeline_id() = std::to_string(value.getSemver().getPipelineId());
  *dest.mutable_job_id() = std::to_string(value.getSemver().getJobId());
  *dest.mutable_project_caption() = value.getProjectCaption();
}

PingRequest Serializer<PingRequest>::fromProtocolBuffer(proto::PingRequest&& source) const {
  return PingRequest(source.id());
}

void Serializer<PingRequest>::moveIntoProtocolBuffer(proto::PingRequest& dest, PingRequest value) const {
  dest.set_id(value.mId);
}

PingResponse Serializer<PingResponse>::fromProtocolBuffer(proto::PingResponse&& source) const {
  PingResponse result{source.id()};
  result.mTimestamp = Serialization::FromProtocolBuffer(
    std::move(*source.mutable_timestamp()));
  return result;
}

void Serializer<PingResponse>::moveIntoProtocolBuffer(proto::PingResponse& dest, PingResponse value) const {
  dest.set_id(value.mId);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_timestamp(), value.mTimestamp);
}

VersionResponse Serializer<VersionResponse>::fromProtocolBuffer(proto::VersionResponse&& source) const {
  std::optional<ConfigVersion> config;
  if (source.has_config_version()) {
    config = Serialization::FromProtocolBuffer(std::move(*source.mutable_config_version()));
  }

  return VersionResponse{
    BinaryVersion(
      std::move(*source.mutable_project_path()),
      std::move(*source.mutable_reference()),
      std::move(*source.mutable_revision()),
      boost::lexical_cast<unsigned int>(source.major_version()),
      boost::lexical_cast<unsigned int>(source.minor_version()),
      boost::lexical_cast<unsigned int>(source.pipeline_id()),
      boost::lexical_cast<unsigned int>(source.job_id()),
      std::move(*source.mutable_target()),
      std::move(*source.mutable_protocol_checksum())),
    config };
}

void Serializer<VersionResponse>::moveIntoProtocolBuffer(proto::VersionResponse& dest, VersionResponse value) const {
  *dest.mutable_project_path() = value.binary.getProjectPath();
  *dest.mutable_reference() = value.binary.getReference();
  *dest.mutable_revision() = value.binary.getRevision();
  *dest.mutable_major_version() = std::to_string(value.binary.getSemver().getMajorVersion());
  *dest.mutable_minor_version() = std::to_string(value.binary.getSemver().getMinorVersion());
  *dest.mutable_pipeline_id() = std::to_string(value.binary.getSemver().getPipelineId());
  *dest.mutable_job_id() = std::to_string(value.binary.getSemver().getJobId());
  *dest.mutable_target() = value.binary.getTarget();
  *dest.mutable_protocol_checksum() = value.binary.getProtocolChecksum();

  if (value.config != std::nullopt) {
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_config_version(), *value.config);
  }
}

}
