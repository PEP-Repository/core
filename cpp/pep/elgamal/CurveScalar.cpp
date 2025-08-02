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
  std::string ret(CurveScalar::PACKEDBYTES, '\0');
  group_scalar_pack(reinterpret_cast<unsigned char*>(ret.data()), &inner);
  return ret;
}

size_t CurveScalar::TextLength() {
  return BoostHexLength(CurveScalar::PACKEDBYTES);
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
CurveScalar::CurveScalar() { //NOLINT(cppcoreguidelines-pro-type-member-init) inner is initialized
  group_scalar_setzero(&inner);
}

CurveScalar CurveScalar::One() {
  CurveScalar scalar;
  group_scalar_setone(&scalar.inner);
  return scalar;
}

/*! \brief Create a new CurveScalar from a packed scalar.
 */
CurveScalar::CurveScalar(std::string_view packed) { //NOLINT(cppcoreguidelines-pro-type-member-init) inner is initialized
  if (packed.size() != CurveScalar::PACKEDBYTES) {
    throw std::invalid_argument("Trying to construct CurveScalar with incorrect number of packed bytes");
  }
  auto error = group_scalar_unpack(&inner, reinterpret_cast<const unsigned char*>(packed.data()));
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
CurveScalar CurveScalar::add(const CurveScalar& s) const {
  CurveScalar r;
  group_scalar_add(&r.inner, &inner, &s.inner);
  return r;
}

/*! \brief Subtract a scalar from this scalar.
 *
 * This scalar remains unchanged.
 *
 * \param s The scalar to add.
 * \return The resulting CurveScalar.
 */
CurveScalar CurveScalar::sub(const CurveScalar& s) const {
  CurveScalar r;
  group_scalar_sub(&r.inner, &inner, &s.inner);
  return r;
}

/*! \brief Multiplies a scalar with this scalar.
 *
 * This scalar remains unchanged.
 *
 * \param s The scalar to multiply with.
 * \return The resulting CurveScalar.
 */
CurveScalar CurveScalar::mult(const CurveScalar& s) const {
  CurveScalar r;
  group_scalar_mul(&r.inner, &inner, &s.inner);
  return r;
}

/*! Squares this scalar.
 *
 * This scalar remains unchanged.
 * \return The resulting CurveScalar
 */
CurveScalar CurveScalar::square() const {
  CurveScalar r;
  group_scalar_square(&r.inner, &inner);
  return r;
}

/*! Calculates the inverse of this scalar.
 *
 * This scalar remains unchanged.
 * \return The resulting CurveScalar.
 */
CurveScalar CurveScalar::invert() const {
  CurveScalar r;
  group_scalar_invert(&r.inner, &inner);
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
  scalar_from64bytes(&r.inner, reinterpret_cast<const unsigned char*>(bytes.data()));
  return r;
}

bool CurveScalar::operator==(const CurveScalar& other) const {
  return group_scalar_equals(&inner, &other.inner);
}

CurveScalar CurveScalar::Random() {
  // Compiler can't seem to deduce template parameter RNG itself
  return CurveScalar::Random<void(uint8_t*,uint64_t)>(RandomBytes);
}

CurveScalar CurveScalar::ShortHash(std::string_view s) {
  CurveScalar ret;
  shortscalar_hashfromstr(
    &ret.inner,
    reinterpret_cast<const unsigned char*>(s.data()),
    s.size()
  );
  return ret;
}

CurveScalar CurveScalar::Hash(std::string_view s) {
  CurveScalar ret;
  scalar_hashfromstr(
    &ret.inner,
    reinterpret_cast<const unsigned char*>(s.data()),
    s.size()
  );
  return ret;
}

}

size_t std::hash<pep::CurveScalar>::operator()(const pep::CurveScalar& k) const {
  return hash<decltype(k.pack())>{}(k.pack()); // TODO prevent copy
}
