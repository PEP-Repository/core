#pragma once

#include <boost/core/noncopyable.hpp>

extern "C" {
#include <panda/group.h>
}

#include <array>
#include <compare>
#include <cstdlib>
#include <string>
#include <string_view>

#include <pep/elgamal/CurveScalar.hpp>
#include <pep/utils/CollectionUtils.hpp>

namespace pep {

// CurvePoints are not completely thread-safe.  See ensureThreadSafe().
class CurvePoint {
 public:
  // The number of bytes in the CurvePoint's packed representation
  static constexpr size_t PACKEDBYTES = GROUP_GE_PACKEDBYTES;

  // Ensures this CurvePoint (also) stores a packed representation.
  //
  // Packing and unpacking CurvePoints is expensive.  That's why this class
  // postpones packing and unpacking CurvePoints until necessary:
  // it has room to store both a packed and unpacked representation of
  // which at least one is always set.
  //
  // After computing on CurvePoints, worker threads will call ensurePacked()
  // such that they perform the computations to pack the point.  If they
  // would not call ensurePacked(), the IO-thread (responsible for
  // serialization) would be required to pack the points, which would stall
  // all requests.
  void ensurePacked() const;

  // If a CurvePoint is read by multiple threads at the same time, which
  // either is not packed or unpacked, then the lazy (un)packing can cause
  // a memory corruption.  See eg #791.
  //
  // Call ensureThreadSafe() before sharing this CurvePoint with multiple
  // threads.  (This only applies to multiple references to the same
  // CurvePoint --- sharing copies with multiple threads is perfectly safe
  // without calling ensureThreadSafe).
  void ensureThreadSafe() const;

  class ScalarMultTable : public boost::noncopyable {
  public:
    explicit ScalarMultTable(const CurvePoint& point);
    CurvePoint mult(const CurveScalar& p) const;
    CurvePoint publicMult(const CurveScalar& p) const;

  private:
    group_scalarmult_table mInternal;
  };

  explicit CurvePoint(std::string_view packed, bool unpack = false);
  /// Construct neutral element
  CurvePoint();

  std::string_view pack() const;

  static size_t TextLength();
  std::string text() const;
  static CurvePoint FromText(const std::string& text);

  CurvePoint add(const CurvePoint& p) const;
  CurvePoint sub(const CurvePoint& p) const;
  CurvePoint dbl() const;
  CurvePoint mult(const CurveScalar& p) const;
  CurvePoint publicMult(const CurveScalar& s) const;

  template<typename RNG>
  static CurvePoint Random(RNG& rng) {
    //NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    std::array<uint8_t, 32> randomBuf;
    rng(randomBuf.data(), randomBuf.size());
    return CurvePoint::Hash(SpanToString(randomBuf));
  }

  static CurvePoint Random();

  static CurvePoint BaseMult(const CurveScalar& s);
  static CurvePoint PublicBaseMult(const CurveScalar& s);
  static CurvePoint Hash(std::string_view s);

  bool isZero() const;

  std::strong_ordering operator<=>(const CurvePoint& other) const;
  bool operator==(const CurvePoint& other) const;

 private:
  // Packing and unpacking CurvePoints is expensive.  That's why we keep
  // the packed/unpacked version around until we really need to
  // unpack/pack it.  And if we need to pack/unpack twice, we already have
  // the value cached.  See also ensurePacked().
  mutable group_ge mUnpacked = group_ge_neutral;
  mutable std::array<char, CurvePoint::PACKEDBYTES> mPacked{};
  enum state_t { gotPacked, gotUnpacked, gotBoth };
  mutable state_t mState;

  // Returns a pointer to the internal unpacked CurvePoint (and unpacks
  // it first, if necessary).
  group_ge* unpack() const;

  explicit CurvePoint(state_t state) : mState(state) { }
};

}

// Specialize std::hash to enable usage of CurvePoint as keys in std::unordered_map
namespace std {
template <> struct hash<pep::CurvePoint> {
  size_t operator()(const pep::CurvePoint& k) const;
};
}
