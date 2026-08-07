#include <pep/server/MonitoringSerializers.hpp>

#include <ranges>

namespace pep {

MetricsResponse Serializer<MetricsResponse>::fromProtocolBuffer(proto::MetricsResponse&& source) const {
  MetricsResponse result;
  result.metrics = std::move(*source.mutable_metrics());
  return result;
}

void Serializer<MetricsResponse>::moveIntoProtocolBuffer(proto::MetricsResponse& dest, MetricsResponse value) const {
  *dest.mutable_metrics() = std::move(value.metrics);
}

ChecksumChainNamesResponse Serializer<ChecksumChainNamesResponse>::fromProtocolBuffer(proto::ChecksumChainNamesResponse&& source) const {
  ChecksumChainNamesResponse result;
  result.names = *source.mutable_names() | std::views::as_rvalue | std::ranges::to<std::vector>();
  return result;
}

void Serializer<ChecksumChainNamesResponse>::moveIntoProtocolBuffer(proto::ChecksumChainNamesResponse& dest, ChecksumChainNamesResponse value) const {
  dest.mutable_names()->Reserve(static_cast<int>(value.names.size()));
  for (auto& name : value.names)
    dest.add_names(std::move(name));
}

ChecksumChainRequest Serializer<ChecksumChainRequest>::fromProtocolBuffer(proto::ChecksumChainRequest&& source) const {
  ChecksumChainRequest result;
  result.name = std::move(*source.mutable_name());
  result.checkpoint = source.checkpoint();
  return result;
}

void Serializer<ChecksumChainRequest>::moveIntoProtocolBuffer(proto::ChecksumChainRequest& dest, ChecksumChainRequest value) const {
  *dest.mutable_name() = std::move(value.name);
  dest.set_checkpoint(value.checkpoint);
}

ChecksumChainResponse Serializer<ChecksumChainResponse>::fromProtocolBuffer(proto::ChecksumChainResponse&& source) const {
  ChecksumChainResponse result;
  result.xorredChecksums = source.xorred_checksums();
  result.checkpoint = source.checkpoint();
  return result;
}

void Serializer<ChecksumChainResponse>::moveIntoProtocolBuffer(proto::ChecksumChainResponse& dest, ChecksumChainResponse value) const {
  dest.set_xorred_checksums(value.xorredChecksums);
  dest.set_checkpoint(value.checkpoint);
}

}
