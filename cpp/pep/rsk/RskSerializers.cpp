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
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_cb(), value.cB_);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_cm(), value.cM_);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_s(), static_cast<const CurveScalar&>(value.mS_));
}

ReshuffleRekeyVerifiers Serializer<ReshuffleRekeyVerifiers>::fromProtocolBuffer(proto::ReshuffleRekeyVerifiers&& source) const {
  return ReshuffleRekeyVerifiers(
    Serialization::FromProtocolBuffer(std::move(*source.mutable_reshuffle_commitment())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_rekey_commitment())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_reshuffle_over_rekey_commitment())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_rekeyed_public_key())));
}
void Serializer<ReshuffleRekeyVerifiers>::moveIntoProtocolBuffer(proto::ReshuffleRekeyVerifiers& dest, ReshuffleRekeyVerifiers value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_reshuffle_commitment(), value.reshuffleCommitment_);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_rekey_commitment(), value.rekeyCommitment_);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_reshuffle_over_rekey_commitment(), value.reshuffleOverRekeyCommitment_);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_rekeyed_public_key(), value.rekeyedPublicKey_);
}

ReshuffleRekeyVerifiersProof Serializer<ReshuffleRekeyVerifiersProof>::fromProtocolBuffer(proto::ReshuffleRekeyVerifiersProof&& source) const {
  return ReshuffleRekeyVerifiersProof(
    Serialization::FromProtocolBuffer(std::move(*source.mutable_rekey_inverse_point())),
    InverseProof(Serialization::FromProtocolBuffer(std::move(*source.mutable_rekey_inverse_proof()))),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_reshuffle_times_rekey_inverse_proof())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_rekey_times_public_key_proof())));
}

void Serializer<ReshuffleRekeyVerifiersProof>::moveIntoProtocolBuffer(proto::ReshuffleRekeyVerifiersProof& dest, ReshuffleRekeyVerifiersProof value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_rekey_inverse_point(), value.rekeyInversePoint_);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_rekey_inverse_proof(), value.rekeyInverseProof_.secretInverseTimesPointProof_);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_reshuffle_times_rekey_inverse_proof(), value.reshuffleTimesRekeyInverseProof_);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_rekey_times_public_key_proof(), value.rekeyTimesPublicKeyProof_);
}

RskProof Serializer<RskProof>::fromProtocolBuffer(proto::RskProof&& source) const {
  return RskProof(
    Serialization::FromProtocolBuffer(std::move(*source.mutable_ry())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_rb())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_rp())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_bp())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_cp()))
  );
}

void Serializer<RskProof>::moveIntoProtocolBuffer(proto::RskProof& dest, RskProof value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ry(), value.rerandomizePubKey_);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_rb(), value.rerandomizePoint_);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_rp(), value.rerandomizeTimesPubKeyProof_);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_bp(), value.reshuffleOverRekeyTimesBProof_);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_cp(), value.reshuffleTimesCProof_);
}

}
