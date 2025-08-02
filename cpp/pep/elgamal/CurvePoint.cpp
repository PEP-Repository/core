#include <pep/elgamal/CurvePoint.hpp>

#include <cassert>
#include <stdexcept>

#include <pep/crypto/ConstTime.hpp>
#include <pep/utils/BoostHexUtil.hpp>
#include <pep/utils/Random.hpp>

#include <boost/algorithm/hex.hpp>

using namespace std::string_literals;

namespace pep {

void CurvePoint::ensureThreadSafe() const {
  // This ensures mPacked and mUnpacked are set (ie. mState == gotBoth).
  ensurePacked();
  unpack();
}

void CurvePoint::ensurePacked() const {
  if (mState != gotUnpacked)
    return;
  group_ge_pack(
      reinterpret_cast<uint8_t*>(mPacked.data()),
      &mUnpacked);
  mState = gotBoth;
}

std::string_view CurvePoint::pack() const {
  ensurePacked();
  return SpanToString(mPacked);
}

group_ge* CurvePoint::unpack() const {
  if (mState == gotPacked) {
    auto error = group_ge_unpack(
      &mUnpacked,
      reinterpret_cast<const uint8_t*>(mPacked.data()));
    if (error != 0) {
      throw std::invalid_argument("Invalid packed CurvePoint");
    }
    mState = gotBoth;
  }
  return &mUnpacked;
}

CurvePoint::CurvePoint(std::string_view packed, bool unpack) {
  if (packed.size() != mPacked.size()) {
    throw std::invalid_argument("Trying to construct CurvePoint with incorrect number of packed bytes");
  }
  std::copy(
    packed.begin(),
    packed.begin() + static_cast<ptrdiff_t>(mPacked.size()),
    mPacked.begin());
  mState = gotPacked;

  if (unpack) {
    this->unpack();
  }
}

CurvePoint::CurvePoint() : mState{gotBoth} {}



/*! \brief Add a CurvePoint to this CurvePoint.
 *
 * This CurvePoint remains unaltered.
 * \param p The CurvePoint to add.
 * \return The resulting CurvePoint.
 */
CurvePoint CurvePoint::add(const CurvePoint& p) const {
  CurvePoint r(gotUnpacked);
  group_ge_add(&r.mUnpacked, unpack(), p.unpack());
  return r;
}

/*! \brief Subtract a CurvePoint from this CurvePoint.
 *
 * This CurvePoint remains unaltered.
 * \param p The CurvePoint to subtract.
 * \return The resulting CurvePoint.
 */
CurvePoint CurvePoint::sub(const CurvePoint& p) const {
  CurvePoint r(gotUnpacked);
  group_ge t;
  group_ge_negate(&t, p.unpack());
  group_ge_add(&r.mUnpacked, unpack(), &t);
  return r;
}

/*! \brief Double this CurvePoint
 *
 * This CurvePoint remains unaltered.
 * \return The resulting CurvePoint.
 */
CurvePoint CurvePoint::dbl() const {
  CurvePoint r(gotUnpacked);
  group_ge_double(&r.mUnpacked, unpack());
  return r;
}

/*! \brief Multiply this CurvePoint with a CurveScalar
 *
 * This CurvePoint remains unaltered.
 * \param p The value to multiply with.
 * \return The resulting CurvePoint.
 */
CurvePoint CurvePoint::mult(const CurveScalar& p) const {
  CurvePoint r(gotUnpacked);
  group_ge_scalarmult(&r.mUnpacked, unpack(), &p.inner);
  return r;
}

/*! \brief Multiply this CurvePoint with a public (not secret!) CurveScalar
 *         You probably want to use CurvePoint::mult instead.
 *
 * This CurvePoint remains unaltered.
 * \param s The value to multiply with.  Must not be a secret!
 * \return The resulting CurvePoint.
 */
CurvePoint CurvePoint::publicMult(const CurveScalar& s) const {
  CurvePoint r(gotUnpacked);
  group_ge_scalarmult_publicinputs(&r.mUnpacked, unpack(), &s.inner);
  return r;
}

/*! \brief Derive CurvePoint from a string
 *
 * The string is hashed using SHA512 and then embedded into the group
 * using the Ristretto variant of Elligator2.
 *
 * \return The derived CurvePoint.
 */
CurvePoint CurvePoint::Hash(std::string_view s) {
  CurvePoint r(gotUnpacked);
  group_ge_hashfromstr(&r.mUnpacked, reinterpret_cast<const unsigned char*>(s.data()), s.length());
  return r;
}

/*! \brief Create a point by multiplying a scalar with the base.
 *
 * \param p The value to multiply the base with.
 * \return The resulting CurvePoint.
 */
CurvePoint CurvePoint::BaseMult(const CurveScalar& p) {
  CurvePoint r(gotUnpacked);
  group_ge_scalarmult_base(&r.mUnpacked, &p.inner);
  return r;
}

/*! \brief Create a point by multiplying a public (not secret!) scalar with the base.
 *         You probably want to use CurvePoint::BaseMult instead
 *
 * \param p The public value to multiply the base with.
 * \return The resulting CurvePoint.
 */
CurvePoint CurvePoint::PublicBaseMult(const CurveScalar& p) {
  CurvePoint r(gotUnpacked);
  group_ge_scalarmult_base_publicinputs(&r.mUnpacked, &p.inner);
  return r;
}

bool CurvePoint::operator== (const CurvePoint& other) const {
  if ((mState == gotPacked || mState == gotBoth)
      && (other.mState == gotPacked || other.mState == gotBoth)) {
    return mPacked == other.mPacked; //TODO why is this not constant time while isZero is?
  }
  return group_ge_equals(unpack(), other.unpack()) == 1;
}

std::strong_ordering CurvePoint::operator<=>(const CurvePoint& other) const {
  return pack() <=> other.pack(); //TODO why is this not constant time while isZero is?
}

bool CurvePoint::isZero() const {
  if (mState == gotPacked || mState == gotBoth) {
    return const_time::IsZero(mPacked);
  }
  return group_ge_equals(unpack(), &group_ge_neutral) == 1;
}

size_t CurvePoint::TextLength() {
  return BoostHexLength(CurvePoint::PACKEDBYTES);
}

std::string CurvePoint::text() const {
  std::string result = boost::algorithm::hex(std::string(pack()));
  assert(result.size() == TextLength());
  return result;
}

CurvePoint CurvePoint::FromText(const std::string& text) {
  try {
    return CurvePoint(boost::algorithm::unhex(text));
  } catch (const boost::algorithm::hex_decode_error& ex) {
    throw std::invalid_argument("CurvePoint text representation is not valid hexadecimal\n"s + ex.what());
  }
}

CurvePoint::ScalarMultTable::ScalarMultTable(const CurvePoint& point) { //NOLINT(cppcoreguidelines-pro-type-member-init) mInternal is initialized
  group_scalarmult_table_compute(&mInternal, point.unpack());
}

CurvePoint CurvePoint::ScalarMultTable::mult(const CurveScalar& s) const {
  CurvePoint r(gotUnpacked);
  group_ge_scalarmult_table(&r.mUnpacked, &mInternal, &s.inner);
  return r;
}

CurvePoint CurvePoint::ScalarMultTable::publicMult(const CurveScalar& s) const {
  CurvePoint r(gotUnpacked);
  group_ge_scalarmult_table_publicinputs(&r.mUnpacked, &mInternal, &s.inner);
  return r;
}

CurvePoint CurvePoint::Random() {
  // Compiler can't seem to deduce template parameter RNG itself
  return CurvePoint::Random<void(uint8_t*,uint64_t)>(RandomBytes);
}

}

size_t std::hash<pep::CurvePoint>::operator()(const pep::CurvePoint& k) const {
  return hash<decltype(k.pack())>{}(k.pack());
}
