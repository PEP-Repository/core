#include <pep/elgamal/CurvePoint.hpp>

#include <cassert>
#include <stdexcept>

#include <pep/crypto/ConstTime.hpp>
#include <pep/utils/BoostHexUtil.hpp>
#include <pep/utils/Random.hpp>

#include <boost/algorithm/hex.hpp>

using namespace std::string_literals;

namespace pep {
namespace {
const CurvePoint BasePoint(PublicCurveScalar(CurveScalar::One()) * CurvePoint::Base);
}

void CurvePoint::ensureThreadSafe() const {
  // This ensures packed_ and unpacked_ are set (ie. state_ == State::GotBoth).
  ensurePacked();
  unpack();
}

void CurvePoint::ensurePacked() const {
  if (state_ != State::GotUnpacked)
    return;
  group_ge_pack(
      reinterpret_cast<uint8_t*>(packed_.data()),
      &unpacked_);
  state_ = State::GotBoth;
}

std::string_view CurvePoint::pack() const {
  ensurePacked();
  return SpanToString(packed_);
}

group_ge* CurvePoint::unpack() const {
  if (state_ == State::GotPacked) {
    auto error = group_ge_unpack(
      &unpacked_,
      reinterpret_cast<const uint8_t*>(packed_.data()));
    if (error != 0) {
      throw std::invalid_argument("Invalid packed CurvePoint");
    }
    state_ = State::GotBoth;
  }
  return &unpacked_;
}

CurvePoint::CurvePoint(std::string_view packed, bool unpack) {
  if (packed.size() != packed_.size()) {
    throw std::invalid_argument("Trying to construct CurvePoint with incorrect number of packed bytes");
  }
  std::copy(
    packed.begin(),
    packed.begin() + static_cast<ptrdiff_t>(packed_.size()),
    packed_.begin());
  state_ = State::GotPacked;

  if (unpack) {
    this->unpack();
  }
}

CurvePoint::CurvePoint() : state_{State::GotBoth} {}

CurvePoint::CurvePoint(BaseT) : CurvePoint(BasePoint) {}


/*! \brief Add a CurvePoint to this CurvePoint.
 *
 * This CurvePoint remains unaltered.
 * \param p The CurvePoint to add.
 * \return The resulting CurvePoint.
 */
CurvePoint CurvePoint::operator+(const CurvePoint& p) const {
  CurvePoint r(State::GotUnpacked);
  group_ge_add(&r.unpacked_, unpack(), p.unpack());
  return r;
}

/*! \brief Subtract a CurvePoint from this CurvePoint.
 *
 * This CurvePoint remains unaltered.
 * \param p The CurvePoint to subtract.
 * \return The resulting CurvePoint.
 */
CurvePoint CurvePoint::operator-(const CurvePoint& p) const {
  CurvePoint r(State::GotUnpacked);
  group_ge t;
  group_ge_negate(&t, p.unpack());
  group_ge_add(&r.unpacked_, unpack(), &t);
  return r;
}

/*! \brief Double this CurvePoint
 *
 * This CurvePoint remains unaltered.
 * \return The resulting CurvePoint.
 */
CurvePoint CurvePoint::dbl() const {
  CurvePoint r(State::GotUnpacked);
  group_ge_double(&r.unpacked_, unpack());
  return r;
}

CurvePoint CurvePoint::mult(const CurveScalar& s) const {
  CurvePoint r(State::GotUnpacked);
  group_ge_scalarmult(&r.unpacked_, unpack(), &s.inner);
  return r;
}

CurvePoint CurvePoint::mult(const PublicCurveScalar& s) const {
  CurvePoint r(State::GotUnpacked);
  group_ge_scalarmult_publicinputs(&r.unpacked_, unpack(), &s.inner);
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
  CurvePoint r(State::GotUnpacked);
  group_ge_hashfromstr(&r.unpacked_, reinterpret_cast<const unsigned char*>(s.data()), s.length());
  return r;
}

CurvePoint CurvePoint::BaseMult(const CurveScalar& s) {
  CurvePoint r(State::GotUnpacked);
  group_ge_scalarmult_base(&r.unpacked_, &s.inner);
  return r;
}

CurvePoint CurvePoint::BaseMult(const PublicCurveScalar& s) {
  CurvePoint r(State::GotUnpacked);
  group_ge_scalarmult_base_publicinputs(&r.unpacked_, &s.inner);
  return r;
}

bool CurvePoint::operator== (const CurvePoint& other) const {
  if ((state_ == State::GotPacked || state_ == State::GotBoth)
      && (other.state_ == State::GotPacked || other.state_ == State::GotBoth)) {
    return packed_ == other.packed_; //TODO why is this not constant time while isZero is?
  }
  return group_ge_equals(unpack(), other.unpack()) == 1;
}

std::strong_ordering CurvePoint::operator<=>(const CurvePoint& other) const {
  return pack() <=> other.pack(); //TODO why is this not constant time while isZero is?
}

bool CurvePoint::isZero() const {
  if (state_ == State::GotPacked || state_ == State::GotBoth) {
    return const_time::IsZero(packed_);
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

CurvePoint::ScalarMultTable::ScalarMultTable(const CurvePoint& point) { //NOLINT(cppcoreguidelines-pro-type-member-init) internal_ is initialized
  group_scalarmult_table_compute(&internal_, point.unpack());
}

CurvePoint CurvePoint::ScalarMultTable::mult(const CurveScalar& s) const {
  CurvePoint r(State::GotUnpacked);
  group_ge_scalarmult_table(&r.unpacked_, &internal_, &s.inner);
  return r;
}

CurvePoint CurvePoint::ScalarMultTable::mult(const PublicCurveScalar& s) const {
  CurvePoint r(State::GotUnpacked);
  group_ge_scalarmult_table_publicinputs(&r.unpacked_, &internal_, &s.inner);
  return r;
}

CurvePoint CurvePoint::Random() {
  return CurvePoint::Hash(SpanToString(RandomArray<32>()));
}

}

size_t std::hash<pep::CurvePoint>::operator()(const pep::CurvePoint& k) const {
  return hash<decltype(k.pack())>{}(k.pack());
}
