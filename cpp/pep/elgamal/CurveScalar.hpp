#pragma once

extern "C" {
#include <panda/scalar.h>
}

#include <array>
#include <compare>
#include <cstdint>
#include <string>
#include <string_view>

#include <pep/utils/CollectionUtils.hpp>

namespace pep {

// Generally secret
class CurveScalar {
  friend class CurvePoint;
public:
  static constexpr size_t PACKEDBYTES = GROUP_SCALAR_PACKEDBYTES;

  CurveScalar();
  explicit CurveScalar(std::string_view packed);

  std::string pack() const;

  static size_t TextLength();
  std::string text() const;
  static CurveScalar FromText(const std::string& text);

  [[nodiscard]] CurveScalar operator+(const CurveScalar& s) const;
  [[nodiscard]] CurveScalar operator-(const CurveScalar& s) const;
  [[nodiscard]] CurveScalar operator*(const CurveScalar& s) const;
  [[nodiscard]] CurveScalar invert() const;
  [[nodiscard]] CurveScalar square() const;

  static CurveScalar One();

  static CurveScalar Random();

  static CurveScalar From64Bytes(std::string_view bytes);

  // Derive a scalar by hashing some data.
  static CurveScalar Hash(std::string_view s);

  // Derive a half-length scalar by hashing some data.
  //
  // WARNING In almost every situation it is insecure to  use a half-length
  //         scalar.  For instance, a half-length (=128 bit) private key only
  //         offers 64bit security.
  static CurveScalar ShortHash(std::string_view s);

  // If you feel like you need to add an ordered comparison operator,
  // you're probably doing something wrong: the time an algorithm (e.g. sorting) takes
  // shouldn't depend on the value of a secret scalar
  bool operator==(const CurveScalar& other) const;

protected:
  group_scalar inner;
};

/// A public (not secret) curve scalar.
/// Will select faster non-constant time algorithms in some cases.
class PublicCurveScalar : public CurveScalar {
public:
  using CurveScalar::CurveScalar;
  explicit PublicCurveScalar(const CurveScalar& s) : CurveScalar(s) {}
};

}

namespace std {
//TODO Use secure hash function for secret scalars.
// Insertion & lookup time will otherwise depend on the value of the (secret!) scalar and scalars already in the map
template <> struct hash<pep::CurveScalar> {
  size_t operator()(const pep::CurveScalar& k) const;
};
}
