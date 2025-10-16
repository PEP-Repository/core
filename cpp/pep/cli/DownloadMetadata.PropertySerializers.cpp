#include <pep/cli/DownloadMetadata.PropertySerializers.hpp>
#include <pep/rsk-pep/Pseudonyms.PropertySerializers.hpp>

namespace pep {

ElgamalEncryption PropertySerializer<ElgamalEncryption>::read(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const {
  return ElgamalEncryption::FromText(DeserializeProperties<std::string>(source, transform));
}

void PropertySerializer<ElgamalEncryption>::write(boost::property_tree::ptree& destination, const ElgamalEncryption& value) const {
  SerializeProperties(destination, value.text());
}

Timestamp PropertySerializer<Timestamp>::read(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const {
  using namespace std::chrono;
  return Timestamp(milliseconds{DeserializeProperties<milliseconds::rep>(source, transform)});
}

void PropertySerializer<Timestamp>::write(boost::property_tree::ptree& destination, const Timestamp& value) const {
  using namespace std::chrono;
  SerializeProperties(destination, value.ticks_since_epoch<milliseconds>());
}

cli::ParticipantIdentifier PropertySerializer<cli::ParticipantIdentifier>::read(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const {
  auto polymorphic = DeserializeProperties<PolymorphicPseudonym>(source, "polymorphic", transform);
  auto local = DeserializeProperties<LocalPseudonym>(source, "local", transform);
  return cli::ParticipantIdentifier(polymorphic, local);
}

void PropertySerializer<cli::ParticipantIdentifier>::write(boost::property_tree::ptree& destination, const cli::ParticipantIdentifier& value) const {
  SerializeProperties(destination, "polymorphic", value.getPolymorphicPseudonym());
  SerializeProperties(destination, "local", value.getLocalPseudonym());
}

cli::RecordDescriptor PropertySerializer<cli::RecordDescriptor>::read(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const {
  auto participant = DeserializeProperties<cli::ParticipantIdentifier>(source, "participant", transform);
  auto column = DeserializeProperties<std::string>(source, "column", transform);
  auto blindingTimestamp = DeserializeProperties<Timestamp>(source, "timestamp", transform); // Not named "blinding-timestamp" or the likes to keep key name backward compatible
  auto payloadBlindingTimestamp = DeserializeProperties<std::optional<Timestamp>>(source, "payload-blinding-timestamp", transform);
  return cli::RecordDescriptor(participant, column, blindingTimestamp, payloadBlindingTimestamp);
}

void PropertySerializer<cli::RecordDescriptor>::write(boost::property_tree::ptree& destination, const cli::RecordDescriptor& value) const {
  SerializeProperties(destination, "participant", value.getParticipant());
  SerializeProperties(destination, "column", value.getColumn());
  SerializeProperties(destination, "timestamp", value.getBlindingTimestamp()); // Not named "blinding-timestamp" or the likes to keep key name backward compatible
  SerializeProperties(destination, "payload-blinding-timestamp", value.getPayloadBlindingTimestamp());
}

cli::RecordState PropertySerializer<cli::RecordState>::read(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const {
  auto descriptor = DeserializeProperties<cli::RecordDescriptor>(source, "descriptor", transform);
  auto hash = DeserializeProperties<std::optional<XxHasher::Hash>>(source, "hash", transform);
  return cli::RecordState{ descriptor, hash };
}

void PropertySerializer<cli::RecordState>::write(boost::property_tree::ptree& destination, const cli::RecordState& value) const {
  SerializeProperties(destination, "descriptor", value.descriptor);
  SerializeProperties(destination, "hash", value.hash);
}

}
