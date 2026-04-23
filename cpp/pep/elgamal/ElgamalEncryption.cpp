#include <pep/elgamal/ElgamalEncryption.hpp>

#include <pep/utils/OpenSSLHasher.hpp>

#include <boost/container_hash/hash.hpp>

#include <sstream>

namespace pep {

namespace {

const char ELGAMAL_ENCRYPTION_TEXT_DELIMITER = ':';

}

std::pair<ElgamalPrivateKey, ElgamalPublicKey> ElgamalEncryption::CreateKeyPair() {
  ElgamalPrivateKey sk = ElgamalPrivateKey::Random();
  ElgamalPublicKey pk = sk * CurvePoint::Base;
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
  b = k * CurvePoint::Base;
  c = data + (k * publicKey);
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
  return c - (sk * b);
}

/*! \brief rerandomize an ElgamalEncryption triple.
 *
 * PRE: (b,c,y) = EG(k,M,y)
 * POST: (b',c',y') = EG(k+z,M,y) for random z
 * The original point is not changed.
 * \return The rerandomized triple.
 * \warning It is important to check that the publicKey is nonzero, otherwise rerandomization is a no-op, which enables other attacks in a full RSK.
 */
ElgamalEncryption ElgamalEncryption::rerandomize() const {
  auto rerandomize = CurveScalar::Random();
  return {
    b + (rerandomize * CurvePoint::Base),
    c + (rerandomize * publicKey),
    publicKey,
  };
}

/*! \brief rekey an ElgamalEncryption triple.
 *
 * PRE: (b,c,y) = EG(k,M,y)
 * POST: (b',c',y') = EG(1/rekey*k,M,rekey*y)
 * The original point is not changed.
 * \param rekey The translation key.
 * \return The rekeyed triple.
 * \warning This should usually be combined with rerandomization to mitigate attacks (not just on unlinkability, see crypto docs).
 */
ElgamalEncryption ElgamalEncryption::rekey(const ElgamalTranslationKey& rekey) const {
  return {
    rekey.invert() * b,
    c,
    rekey * publicKey,
  };
}

/*! \brief reshuffle an ElgamalEncryption triple.
 *
 * PRE: (b,c,y) = EG(k,M,y)
 * POST: (b',c',y') = EG(reshuffle*k,reshuffle*M,y)
 * The original point is not changed.
 * \param reshuffle The value to reshuffle with.
 * \return The reshuffled triple.
 * \warning This should usually be combined with rerandomization to mitigate attacks (not just on unlinkability, see crypto docs).
 */
ElgamalEncryption ElgamalEncryption::reshuffle(const CurveScalar& reshuffle) const {
  return {
    reshuffle * b,
    reshuffle * c,
    publicKey,
  };
}

/*! \brief reshuffle and rekey an ElgamalEncryption triple.
 *
 * \param reshuffle the CurveScalar to reshuffle with
 * \param rekey the ElgamalTranslationKey to rekey along
 * \warning This should usually be combined with rerandomization to mitigate attacks (not just on unlinkability, see crypto docs).
 */
ElgamalEncryption ElgamalEncryption::reshuffleRekey(const CurveScalar& reshuffle, const ElgamalTranslationKey& rekey) const {
  return {
    reshuffle * rekey.invert() * b,
    reshuffle * c,
    rekey * publicKey,
  };
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
