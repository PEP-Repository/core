#include <pep/elgamal/ElgamalEncryption.hpp>

#include <pep/utils/Sha.hpp>

#include <boost/container_hash/hash.hpp>

#include <sstream>

namespace pep {

namespace {

const char ELGAMAL_ENCRYPTION_TEXT_DELIMITER = ':';

}

std::pair<ElgamalPrivateKey, ElgamalPublicKey> ElgamalEncryption::CreateKeyPair() {
  ElgamalPrivateKey sk = ElgamalPrivateKey::Random();
  ElgamalPublicKey pk = ElgamalPublicKey::BaseMult(sk);
  return {sk, pk};
}

/*! \brief Create an ElgamalEncryption triple by encrypting a point.
 *
 * \param publicKey The public key to encrypt with.
 * \param data The point to encrypt.
 */
ElgamalEncryption::ElgamalEncryption(const ElgamalPublicKey& publicKey, const CurvePoint& data) {
  CurveScalar k;

  k = CurveScalar::Random();
  b = CurvePoint::BaseMult(k);
  c = data + (publicKey * k);
  this->publicKey = publicKey;
}

/*! \brief Create an ElgamalEncryption triple from its three components.
 *
 * \param b Blinding component
 * \param c Cipher component
 * \param publicKey Public key
 */
ElgamalEncryption::ElgamalEncryption(const CurvePoint& b, const CurvePoint& c, const CurvePoint& publicKey)
  : b(b), c(c), publicKey(publicKey) {
}

/*! \brief Decrypt the ElgamalEncryption triple.
 *
 * \param sk The private key to decrypt with.
 * \return The decrypted point.
 */
CurvePoint ElgamalEncryption::decrypt(const ElgamalPrivateKey& sk) const {
  return c - (b * sk);
}

/*! \brief rerandomize an ElgamalEncryption triple.
 *
 * PRE: (b,c,y) = EG(k,M,y)
 * POST: (b',c',y') = EG(k+z,M,y) for random z
 * The original point is not changed.
 * \return The rerandomized triple.
 */
ElgamalEncryption ElgamalEncryption::rerandomize() const {
  ElgamalEncryption r;

  CurveScalar z = CurveScalar::Random();
  // (a, b) =
  // (g * k, s + g * x * k)
  // goal : transform k to k + z
  // transform to :
  // (g * (k + z), s + g * x * (k + z)) =
  // (g * k + g * z, s + g * x * k + g * x * z) =
  // (a + g * z, b + g * x * z)

  r.b = b + CurvePoint::BaseMult(z);
  r.c = c + (publicKey * z);

  r.publicKey = publicKey;
  return r;
}

/*! \brief rekey an ElgamalEncryption triple.
 *
 * PRE: (b,c,y) = EG(k,M,y)
 * POST: (b',c',y') = EG(1/z*k,M,z*y)
 * The original point is not changed.
 * \param z The translation key.
 * \return The rekeyed triple.
 */
ElgamalEncryption ElgamalEncryption::rekey(const ElgamalTranslationKey& z) const {
  ElgamalEncryption r;
  // (a, b) =
  // (g * k, s + g * x * k)
  // goal : ability to decrypt with key x + z
  // transform to :
  // (g * k, s + g * (x + z) * k) =
  // (g * k, s + g * k * x + g * k * z =
  // (a, b + a * z)
  r.b = b * z.invert();
  r.c = c;
  r.publicKey = publicKey * z;
  return r;
}

/*! \brief reshuffle an ElgamalEncryption triple.
 *
 * PRE: (b,c,y) = EG(k,M,y)
 * POST: (b',c',y') = EG(z*k,z*M,y)
 * The original point is not changed.
 * \param z The value to reshuffle with.
 * \return The reshuffled triple.
 */
ElgamalEncryption ElgamalEncryption::reshuffle(const CurveScalar& z) const {
  ElgamalEncryption r;
  // (a, b) =
  // (g * k, s + g * x * k)
  // goal : transform s to s * z
  // transform to:
  // (g * k * z, s * z + g * x * k * z) =
  // (a * z, b * z)
  r.b = b * z;
  r.c = c * z;
  r.publicKey = publicKey;
  return r;
}

/*! \brief rerandomize, reshuffle and rekey an ElgamalEncryption triple.
 *
 *  \param z the CurveScalar to reshuffle with
 *  \param k the ElgamalTranslationKey to rekey along
 *
 *  Note: it is important to check that y is non-zero --- otherwise information
 *        about z and k might leak.
 */
ElgamalEncryption ElgamalEncryption::RSK(const CurveScalar& z, const ElgamalTranslationKey& k) const {
  //  (b, c, y)
  //     |
  //     |  rerandomize with r
  //     V
  //  (b + rB, c + ry, y)
  //     |
  //     |  reshuffle with z
  //     V
  //  (z (b + rB), z (c + ry), y)
  //     |
  //     |  rekey with k
  //     V
  //  ( (z/k) (b + rB), z (c + ry), ky)

  // XXX we can reuse the precomputation of the multiples for y.
  //     (ie. cache window3() in Panda)


  auto r = CurveScalar::Random();
  auto rB = CurvePoint::BaseMult(r);
  auto ry = publicKey * r;
  auto zOverK = z * k.invert();

  ElgamalEncryption ret;
  ret.b = (b + rB) * zOverK;
  ret.c = (c + ry) * z;
  ret.publicKey = publicKey * k;
  return ret;
}

/*!
 * \return The public key of the ElgamalEncryption.
 */
const ElgamalPublicKey& ElgamalEncryption::getPublicKey() const {
  return publicKey;
}

size_t ElgamalEncryption::TextLength() {
  // Ideally we'd calculate ELGAMAL_ENCRYPTION_TEXT_DELIMITER's length on the fly, but we'd then want to be prepared
  // for its type changing as well (from char to e.g. wchar_t , or char* , or std::string, or std::wstring, or...).
  // Since I don't want to bother with writing all the required support code, I just use the magic constant "2" below.
  // This static_assert ensures that it has the correct value.
  static_assert(std::is_same_v<const char, decltype(ELGAMAL_ENCRYPTION_TEXT_DELIMITER)>,
    "ElGamal encryption text length calculation depends on the use of single-character delimiters");

  return 3 * CurvePoint::TextLength() + 2;
}

std::string ElgamalEncryption::text() const {
  auto result = b.text() + ELGAMAL_ENCRYPTION_TEXT_DELIMITER + c.text() + ELGAMAL_ENCRYPTION_TEXT_DELIMITER + publicKey.text();
  assert(result.size() == TextLength());
  return result;
}

ElgamalEncryption ElgamalEncryption::FromText(const std::string& text) {
  std::istringstream ss(text);
  std::string tb, tc, ty;
  std::getline(ss, tb, ELGAMAL_ENCRYPTION_TEXT_DELIMITER);
  std::getline(ss, tc, ELGAMAL_ENCRYPTION_TEXT_DELIMITER);
  ss >> ty;
  return ElgamalEncryption(CurvePoint::FromText(tb), CurvePoint::FromText(tc), CurvePoint::FromText(ty));
}

std::string ElgamalEncryption::pack() const {
  std::string packed;
  packed.reserve(CurvePoint::PACKEDBYTES * 3);
  packed += b.pack();
  packed += c.pack();
  packed += publicKey.pack();
  return packed;
}

ElgamalEncryption ElgamalEncryption::FromPacked(std::string_view packed) {
  return {
      CurvePoint(packed.substr(CurvePoint::PACKEDBYTES * 0, CurvePoint::PACKEDBYTES)),
      CurvePoint(packed.substr(CurvePoint::PACKEDBYTES * 1, CurvePoint::PACKEDBYTES)),
      CurvePoint(packed.substr(CurvePoint::PACKEDBYTES * 2, CurvePoint::PACKEDBYTES)),
  };
}

void ElgamalEncryption::ensurePacked() const {
  b.ensurePacked();
  c.ensurePacked();
  publicKey.ensurePacked();
}

void ElgamalEncryption::ensureThreadSafe() const {
  b.ensureThreadSafe();
  c.ensureThreadSafe();
  publicKey.ensureThreadSafe();
}

}

namespace std {
  size_t hash<pep::ElgamalEncryption>::operator()(const pep::ElgamalEncryption& k) const {
    size_t result{};
    std::hash<pep::CurvePoint> h;
    boost::hash_combine(result, h(k.b));
    boost::hash_combine(result, h(k.c));
    boost::hash_combine(result, h(k.publicKey));
    return result;
  }
}
