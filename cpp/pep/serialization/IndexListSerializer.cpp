#include <pep/serialization/IndexListSerializer.hpp>

namespace pep {

IndexList Serializer<IndexList>::fromProtocolBuffer(proto::IndexList&& source) const {
  IndexList result;
  result.mIndices = std::vector<uint32_t>(
    source.indices().begin(),
    source.indices().end());
  return result;
}

void Serializer<IndexList>::moveIntoProtocolBuffer(proto::IndexList& dest, IndexList value) const {
  dest.mutable_indices()->Reserve(static_cast<int>(value.mIndices.size()));
  for (auto x : value.mIndices)
    dest.mutable_indices()->AddAlreadyReserved(x);
}

}
