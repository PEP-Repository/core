#include <pep/server/MonitoringSerializers.hpp>

namespace pep {

MetricsResponse Serializer<MetricsResponse>::fromProtocolBuffer(proto::MetricsResponse&& source) const {
  MetricsResponse result;
  result.mMetrics = std::move(*source.mutable_metrics());
  return result;
}

void Serializer<MetricsResponse>::moveIntoProtocolBuffer(proto::MetricsResponse& dest, MetricsResponse value) const {
  *dest.mutable_metrics() = std::move(value.mMetrics);
}

ChecksumChainNamesResponse Serializer<ChecksumChainNamesResponse>::fromProtocolBuffer(proto::ChecksumChainNamesResponse&& source) const {
  ChecksumChainNamesResponse result;
  result.mNames.reserve(static_cast<size_t>(source.names().size()));
  for (auto& name : *source.mutable_names())
    result.mNames.push_back(std::move(name));
  return result;
}

void Serializer<ChecksumChainNamesResponse>::moveIntoProtocolBuffer(proto::ChecksumChainNamesResponse& dest, ChecksumChainNamesResponse value) const {
  dest.mutable_names()->Reserve(static_cast<int>(value.mNames.size()));
  for (auto& name : value.mNames)
    dest.add_names(std::move(name));
}

ChecksumChainRequest Serializer<ChecksumChainRequest>::fromProtocolBuffer(proto::ChecksumChainRequest&& source) const {
  ChecksumChainRequest result;
  result.mName = std::move(*source.mutable_name());
  result.mCheckpoint = source.checkpoint();
  return result;
}

void Serializer<ChecksumChainRequest>::moveIntoProtocolBuffer(proto::ChecksumChainRequest& dest, ChecksumChainRequest value) const {
  *dest.mutable_name() = std::move(value.mName);
  dest.set_checkpoint(value.mCheckpoint);
}

ChecksumChainResponse Serializer<ChecksumChainResponse>::fromProtocolBuffer(proto::ChecksumChainResponse&& source) const {
  ChecksumChainResponse result;
  result.mXorredChecksums = source.xorredchecksums();
  result.mCheckpoint = source.checkpoint();
  return result;
}

void Serializer<ChecksumChainResponse>::moveIntoProtocolBuffer(proto::ChecksumChainResponse& dest, ChecksumChainResponse value) const {
  dest.set_xorredchecksums(value.mXorredChecksums);
  dest.set_checkpoint(value.mCheckpoint);
}

}
