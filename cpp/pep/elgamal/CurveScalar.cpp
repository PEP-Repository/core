#include <pep/elgamal/CurveScalar.hpp>

#include <cassert>
#include <cstring>
#include <cstdlib>

#include <pep/utils/BoostHexUtil.hpp>
#include <pep/utils/Random.hpp>

#include <boost/algorithm/hex.hpp>

using namespace std::string_literals;

namespace pep {

std::string CurveScalar::pack() const {
  std::string ret(CurveScalar::PackedBytes, '\0');
  group_scalar_pack(reinterpret_cast<unsigned char*>(ret.data()), &inner_);
  return ret;
}

size_t CurveScalar::TextLength() {
  return BoostHexLength(CurveScalar::PackedBytes);
}

std::string CurveScalar::text() const {
  std::string result = boost::algorithm::hex(pack());
  assert(result.size() == TextLength());
  return result;
}

CurveScalar CurveScalar::FromText(const std::string& text) {
  try {
    return CurveScalar(boost::algorithm::unhex(text));
  } catch (const boost::algorithm::hex_decode_error& ex) {
    throw std::invalid_argument("CurveScalar text representation is not valid hexadecimal\n"s + ex.what());
  }
}

/*! \brief Create a zero CurveScalar
 */
CurveScalar::CurveScalar() { //NOLINT(cppcoreguidelines-pro-type-member-init) inner_ is initialized
  group_scalar_setzero(&inner_);
}

CurveScalar CurveScalar::One() {
  CurveScalar scalar;
  group_scalar_setone(&scalar.inner_);
  return scalar;
}

/*! \brief Create a new CurveScalar from a packed scalar.
 */
CurveScalar::CurveScalar(std::string_view packed) { //NOLINT(cppcoreguidelines-pro-type-member-init) inner_ is initialized
  if (packed.size() != CurveScalar::PackedBytes) {
    throw std::invalid_argument("Trying to construct CurveScalar with incorrect number of packed bytes");
  }
  auto error = group_scalar_unpack(&inner_, reinterpret_cast<const unsigned char*>(packed.data()));
  if (error != 0) {  // Never happens
    throw std::invalid_argument("Invalid packed CurveScalar");
  }
}

/*! \brief Adds a scalar to this scalar.
 *
 * This scalar remains unchanged.
 *
 * \param s The scalar to add.
 * \return The resulting CurveScalar.
 */
CurveScalar CurveScalar::operator+(const CurveScalar& s) const {
  CurveScalar r;
  group_scalar_add(&r.inner_, &inner_, &s.inner_);
  return r;
}

/*! \brief Subtract a scalar from this scalar.
 *
 * This scalar remains unchanged.
 *
 * \param s The scalar to add.
 * \return The resulting CurveScalar.
 */
CurveScalar CurveScalar::operator-(const CurveScalar& s) const {
  CurveScalar r;
  group_scalar_sub(&r.inner_, &inner_, &s.inner_);
  return r;
}

/*! \brief Multiplies a scalar with this scalar.
 *
 * This scalar remains unchanged.
 *
 * \param s The scalar to multiply with.
 * \return The resulting CurveScalar.
 */
CurveScalar CurveScalar::operator*(const CurveScalar& s) const {
  CurveScalar r;
  group_scalar_mul(&r.inner_, &inner_, &s.inner_);
  return r;
}

/*! Squares this scalar.
 *
 * This scalar remains unchanged.
 * \return The resulting CurveScalar
 */
CurveScalar CurveScalar::square() const {
  CurveScalar r;
  group_scalar_square(&r.inner_, &inner_);
  return r;
}

/*! Calculates the inverse of this scalar.
 *
 * This scalar remains unchanged.
 * \return The resulting CurveScalar.
 */
CurveScalar CurveScalar::invert() const {
  CurveScalar r;
  group_scalar_invert(&r.inner_, &inner_);
  return r;
}

/*! \brief Creates a valid CurveScalar from the specified 64 bytes.
 *
 * The data is modified to comply with the constraints for CurveScalar values (i.e. clamped).
 */
CurveScalar CurveScalar::From64Bytes(std::string_view bytes) {
  if (bytes.size() != 64) {
    throw std::invalid_argument("Trying to construct CurveScalar with incorrect number of bytes");
  }
  CurveScalar r;
  scalar_from64bytes(&r.inner_, reinterpret_cast<const unsigned char*>(bytes.data()));
  return r;
}

bool CurveScalar::operator==(const CurveScalar& other) const {
  return group_scalar_equals(&inner_, &other.inner_);
}

CurveScalar CurveScalar::Random() {
  return CurveScalar::From64Bytes(SpanToString(RandomArray<64>()));
}

CurveScalar CurveScalar::ShortHash(std::string_view s) {
  CurveScalar ret;
  shortscalar_hashfromstr(
    &ret.inner_,
    reinterpret_cast<const unsigned char*>(s.data()),
    s.size()
  );
  return ret;
}

CurveScalar CurveScalar::Hash(std::string_view s) {
  CurveScalar ret;
  scalar_hashfromstr(
    &ret.inner_,
    reinterpret_cast<const unsigned char*>(s.data()),
    s.size()
  );
  return ret;
}

}

size_t std::hash<pep::CurveScalar>::operator()(const pep::CurveScalar& k) const {
  return hash<decltype(k.pack())>{}(k.pack()); // TODO prevent copy
}
