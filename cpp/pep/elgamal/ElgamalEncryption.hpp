#pragma once

#include <pep/elgamal/CurvePoint.hpp>

#include <utility>

namespace pep {
using ElgamalPrivateKey = CurveScalar;
using ElgamalPublicKey = CurvePoint;
using ElgamalTranslationKey = CurveScalar;

// #define MAGIC_ELGAMALENCRYPTION 0xcafef001

/*! \brief An ElGamal Encryption Triple.
 */
class ElgamalEncryption {
 public:
  static constexpr size_t PACKEDBYTES = CurvePoint::PACKEDBYTES * 3;

  ElgamalEncryption(
    const ElgamalPublicKey& pk,
    const CurvePoint& data
  );
  ElgamalEncryption(
    const CurvePoint& b,
    const CurvePoint& c,
    const CurvePoint& y
  );
  ElgamalEncryption() = default;

  CurvePoint decrypt(const ElgamalPrivateKey&) const;

  ElgamalEncryption rerandomize() const;
  ElgamalEncryption rekey(const ElgamalTranslationKey& z) const;
  ElgamalEncryption reshuffle(const CurveScalar& z) const;
  ElgamalEncryption RSK(const CurveScalar& z, const ElgamalTranslationKey& k) const;

  const ElgamalPublicKey& getPublicKey() const;

  static size_t TextLength();
  std::string text() const;
  static ElgamalEncryption FromText(const std::string& text);

  std::string pack() const;
  static ElgamalEncryption FromPacked(std::string_view packed);


  // Ensures the underlying CurvePoint's are pre-packed for serialization.
  // See CurvePoint::ensurePacked().
  void ensurePacked() const;
  void ensureThreadSafe() const;

  [[nodiscard]] auto operator<=>(const ElgamalEncryption& other) const = default;

  /// Generate an Elgamal key pair.
  [[nodiscard]] static std::pair<ElgamalPrivateKey, ElgamalPublicKey> CreateKeyPair();

  CurvePoint b;
  CurvePoint c;
  CurvePoint y;

};

using EncryptedKey = ElgamalEncryption;

}

// Specialize std::hash to enable usage of ElgamalEncryption as keys in std::unordered_map
namespace std {
template <> struct hash<pep::ElgamalEncryption> {
  size_t operator()(const pep::ElgamalEncryption& k) const;
};
}
