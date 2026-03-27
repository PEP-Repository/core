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
    Serialization::FromProtocolBuffer(std::move(*source.mutable_reshuffle_commitment())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_rekey_commitment())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_reshuffle_over_rekey_commitment())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_rekeyed_public_key())));
}
void Serializer<ReshuffleRekeyVerifiers>::moveIntoProtocolBuffer(proto::ReshuffleRekeyVerifiers& dest, ReshuffleRekeyVerifiers value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_reshuffle_commitment(), value.mReshuffleCommitment);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_rekey_commitment(), value.mRekeyCommitment);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_reshuffle_over_rekey_commitment(), value.mReshuffleOverRekeyCommitment);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_rekeyed_public_key(), value.mRekeyedPublicKey);
}

ReshuffleRekeyVerifiersProof Serializer<ReshuffleRekeyVerifiersProof>::fromProtocolBuffer(proto::ReshuffleRekeyVerifiersProof&& source) const {
  return ReshuffleRekeyVerifiersProof(
    Serialization::FromProtocolBuffer(std::move(*source.mutable_rekey_inverse_point())),
    InverseProof(Serialization::FromProtocolBuffer(std::move(*source.mutable_rekey_inverse_proof()))),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_reshuffle_times_rekey_inverse_proof())),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_rekey_times_public_key_proof())));
}

void Serializer<ReshuffleRekeyVerifiersProof>::moveIntoProtocolBuffer(proto::ReshuffleRekeyVerifiersProof& dest, ReshuffleRekeyVerifiersProof value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_rekey_inverse_point(), value.mRekeyInversePoint);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_rekey_inverse_proof(), value.mRekeyInverseProof.mSecretInverseTimesPointProof);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_reshuffle_times_rekey_inverse_proof(), value.mReshuffleTimesRekeyInverseProof);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_rekey_times_public_key_proof(), value.mRekeyTimesPublicKeyProof);
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

}
