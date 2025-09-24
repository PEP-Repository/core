#include <pep/rsk/RskSerializers.hpp>

#include <pep/serialization/Serialization.hpp>
#include <pep/elgamal/ElgamalSerializers.hpp>

#include <stdexcept>
#include <utility>

namespace pep {

ScalarMultProof Serializer<ScalarMultProof>::fromProtocolBuffer(proto::ScalarMultProof&& source) const {
  return ScalarMultProof(
    Serialization::FromProtocolBuffer(std::move(*source.mutable_cb())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_cm())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_s()))
  );
}

void Serializer<ScalarMultProof>::moveIntoProtocolBuffer(proto::ScalarMultProof& dest, ScalarMultProof value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_cb(), value.mCB);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_cm(), value.mCM);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_s(), value.mS);
}

RSKVerifiers Serializer<RSKVerifiers>::fromProtocolBuffer(proto::RSKVerifiers&& source) const {
  return RSKVerifiers(
    Serialization::FromProtocolBuffer(std::move(*source.mutable_z_over_kb())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_zb())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_ky()))
  );
}

void Serializer<RSKVerifiers>::moveIntoProtocolBuffer(proto::RSKVerifiers& dest, RSKVerifiers value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_z_over_kb(), value.mZOverKB);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_zb(), value.mZB);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ky(), value.mKY);
}

RSKProof Serializer<RSKProof>::fromProtocolBuffer(proto::RSKProof&& source) const {
  return RSKProof(
    Serialization::FromProtocolBuffer(std::move(*source.mutable_ry())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_rb())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_rp())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_bp())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_cp()))
  );
}

void Serializer<RSKProof>::moveIntoProtocolBuffer(proto::RSKProof& dest, RSKProof value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ry(), value.mRY);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_rb(), value.mRB);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_rp(), value.mRP);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_bp(), value.mBP);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_cp(), value.mCP);
}

VerifiersResponse Serializer<VerifiersResponse>::fromProtocolBuffer(proto::VerifiersResponse&& source) const {
  return VerifiersResponse(
    Serialization::FromProtocolBuffer(std::move(*source.mutable_access_manager())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_storage_facility())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_transcryptor()))
  );
}

void Serializer<VerifiersResponse>::moveIntoProtocolBuffer(proto::VerifiersResponse& dest, VerifiersResponse value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_access_manager(), value.mAccessManager);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_storage_facility(), value.mStorageFacility);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_transcryptor(), value.mTranscryptor);
}

}
