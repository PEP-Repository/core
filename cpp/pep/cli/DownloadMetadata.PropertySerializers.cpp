#include <pep/cli/DownloadMetadata.PropertySerializers.hpp>
#include <pep/rsk-pep/Pseudonyms.PropertySerializers.hpp>

namespace pep {

ElgamalEncryption PropertySerializer<ElgamalEncryption>::read(const boost::property_tree::ptree& source, const DeserializationContext& context) const {
  return ElgamalEncryption::FromText(DeserializeProperties<std::string>(source, context));
}

void PropertySerializer<ElgamalEncryption>::write(boost::property_tree::ptree& destination, const ElgamalEncryption& value) const {
  SerializeProperties(destination, value.text());
}

Timestamp PropertySerializer<Timestamp>::read(const boost::property_tree::ptree& source, const DeserializationContext& context) const {
  using namespace std::chrono;
  return Timestamp(milliseconds{DeserializeProperties<milliseconds::rep>(source, context)});
}

void PropertySerializer<Timestamp>::write(boost::property_tree::ptree& destination, const Timestamp& value) const {
  using namespace std::chrono;
  SerializeProperties(destination, TicksSinceEpoch<milliseconds>(value));
}

cli::ParticipantIdentifier PropertySerializer<cli::ParticipantIdentifier>::read(const boost::property_tree::ptree& source, const DeserializationContext& context) const {
  auto polymorphic = DeserializeProperties<PolymorphicPseudonym>(source, "polymorphic", context);
  auto local = DeserializeProperties<LocalPseudonym>(source, "local", context);
  return cli::ParticipantIdentifier(polymorphic, local);
}

void PropertySerializer<cli::ParticipantIdentifier>::write(boost::property_tree::ptree& destination, const cli::ParticipantIdentifier& value) const {
  SerializeProperties(destination, "polymorphic", value.getPolymorphicPseudonym());
  SerializeProperties(destination, "local", value.getLocalPseudonym());
}

cli::RecordDescriptor PropertySerializer<cli::RecordDescriptor>::read(const boost::property_tree::ptree& source, const DeserializationContext& context) const {
  auto participant = DeserializeProperties<cli::ParticipantIdentifier>(source, "participant", context);
  auto column = DeserializeProperties<std::string>(source, "column", context);
  auto blindingTimestamp = DeserializeProperties<Timestamp>(source, "timestamp", context); // Not named "blinding-timestamp" or the likes to keep key name backward compatible
  auto payloadBlindingTimestamp = DeserializeProperties<std::optional<Timestamp>>(source, "payload-blinding-timestamp", context);
  return cli::RecordDescriptor(participant, column, blindingTimestamp, payloadBlindingTimestamp);
}

void PropertySerializer<cli::RecordDescriptor>::write(boost::property_tree::ptree& destination, const cli::RecordDescriptor& value) const {
  SerializeProperties(destination, "participant", value.getParticipant());
  SerializeProperties(destination, "column", value.getColumn());
  SerializeProperties(destination, "timestamp", value.getBlindingTimestamp()); // Not named "blinding-timestamp" or the likes to keep key name backward compatible
  SerializeProperties(destination, "payload-blinding-timestamp", value.getPayloadBlindingTimestamp());
}

cli::RecordState PropertySerializer<cli::RecordState>::read(const boost::property_tree::ptree& source, const DeserializationContext& context) const {
  auto descriptor = DeserializeProperties<cli::RecordDescriptor>(source, "descriptor", context);
  auto hash = DeserializeProperties<std::optional<XxHasher::Hash>>(source, "hash", context);
  return cli::RecordState{ descriptor, hash };
}

void PropertySerializer<cli::RecordState>::write(boost::property_tree::ptree& destination, const cli::RecordState& value) const {
  SerializeProperties(destination, "descriptor", value.descriptor);
  SerializeProperties(destination, "hash", value.hash);
}

}
