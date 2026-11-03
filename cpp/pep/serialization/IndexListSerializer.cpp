#include <pep/serialization/IndexListSerializer.hpp>
#include <pep/serialization/IndexList.hpp>

#include <Messages.pb.h> // for proto::IndexList

#include <cstdint>
#include <vector>

namespace pep {

IndexList Serializer<IndexList>::fromProtocolBuffer(proto::IndexList&& source) const {
  IndexList result;
  result.indices = std::vector<uint32_t>(
    source.indices().begin(),
    source.indices().end());
  return result;
}

void Serializer<IndexList>::moveIntoProtocolBuffer(proto::IndexList& dest, IndexList value) const {
  dest.mutable_indices()->Reserve(static_cast<int>(value.indices.size()));
  for (auto x : value.indices)
    dest.mutable_indices()->AddAlreadyReserved(x);
}

}
