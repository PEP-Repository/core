#include <pep/elgamal/ElgamalSerializers.hpp>

#include <pep/serialization/Serialization.hpp>

#include <stdexcept>
#include <utility>

namespace pep {

CurveScalar Serializer<CurveScalar>::fromProtocolBuffer(proto::CurveScalar&& source) const {
  try {
    return CurveScalar(source.curve_scalar());
  } catch (const std::invalid_argument& ex) {
    throw SerializeException(ex.what());
  }
}

void Serializer<CurveScalar>::moveIntoProtocolBuffer(proto::CurveScalar& dest, CurveScalar value) const {
  *dest.mutable_curve_scalar() = value.pack();
}

CurvePoint Serializer<CurvePoint>::fromProtocolBuffer(proto::CurvePoint&& source) const {
  try {
    return CurvePoint(source.curve_point());
  } catch (const std::invalid_argument& ex) {
    throw SerializeException(ex.what());
  }
}

void Serializer<CurvePoint>::moveIntoProtocolBuffer(proto::CurvePoint& dest, CurvePoint value) const {
  auto packed = value.pack();
  dest.set_curve_point(packed.data(), packed.size());
}

ElgamalEncryption Serializer<ElgamalEncryption>::fromProtocolBuffer(proto::ElgamalEncryption&& source) const {
  ElgamalEncryption result;
  result.b = Serialization::FromProtocolBuffer(std::move(*source.mutable_b()));
  result.c = Serialization::FromProtocolBuffer(std::move(*source.mutable_c()));
  result.y = Serialization::FromProtocolBuffer(std::move(*source.mutable_y()));
  return result;
}

void Serializer<ElgamalEncryption>::moveIntoProtocolBuffer(proto::ElgamalEncryption& dest, ElgamalEncryption value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_b(), value.b);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_c(), value.c);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_y(), value.y);
}

} // namespace pep
