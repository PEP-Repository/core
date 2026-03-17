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
    PublicCurveScalar(Serialization::FromProtocolBuffer(std::move(*source.mutable_s())))
  );
}

void Serializer<ScalarMultProof>::moveIntoProtocolBuffer(proto::ScalarMultProof& dest, ScalarMultProof value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_cb(), value.mCB);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_cm(), value.mCM);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_s(), static_cast<const CurveScalar&>(value.mS));
}

ReshuffleRekeyVerifiers Serializer<ReshuffleRekeyVerifiers>::fromProtocolBuffer(proto::ReshuffleRekeyVerifiers&& source) const {
  return ReshuffleRekeyVerifiers(
    Serialization::FromProtocolBuffer(std::move(*source.mutable_z_over_kb())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_zb())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_ky()))
  );
}

void Serializer<ReshuffleRekeyVerifiers>::moveIntoProtocolBuffer(proto::ReshuffleRekeyVerifiers& dest, ReshuffleRekeyVerifiers value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_z_over_kb(), value.mReshuffleOverRekeyPoint);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_zb(), value.mReshufflePoint);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ky(), value.mRekeyedPublicKey);
}

ReshuffleRekeyProof Serializer<ReshuffleRekeyProof>::fromProtocolBuffer(proto::ReshuffleRekeyProof&& source) const {
  return ReshuffleRekeyProof(
    Serialization::FromProtocolBuffer(std::move(*source.mutable_bp())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_cp()))
  );
}

void Serializer<ReshuffleRekeyProof>::moveIntoProtocolBuffer(proto::ReshuffleRekeyProof& dest, ReshuffleRekeyProof value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_bp(), value.mReshuffleOverRekeyTimesBProof);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_cp(), value.mReshuffleTimesCProof);
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
