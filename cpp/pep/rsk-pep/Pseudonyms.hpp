#pragma once

#include <pep/elgamal/CurvePoint.hpp>
#include <pep/elgamal/ElgamalEncryption.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

namespace pep {

class LocalPseudonym final {
  friend std::hash<LocalPseudonym>;

  CurvePoint mPoint;

public:
  /// Construct invalid pseudonym
  LocalPseudonym() = default;

  /// \warning You normally don't need this except for serialization
  /// \throws std::invalid_argument for invalid point
  explicit LocalPseudonym(CurvePoint point);

  /// Get inner \c CurvePoint
  /// \warning You normally don't need this except for serialization
  /// \throws std::invalid_argument for invalid point (zero)
  const CurvePoint& getValidCurvePoint() const;

  [[nodiscard]] static LocalPseudonym Random();

  /// Parse printable (hex) representation from text()
  /// \throws std::invalid_argument for invalid representation
  /// \warning Lazy unpacking may occur, meaning that any other method may also throw if the serialization was invalid
  [[nodiscard]] static LocalPseudonym FromText(std::string_view text);
  /// Get length of string returned by text()
  [[nodiscard]] static size_t TextLength();
  /// Get printable (hex) representation
  /// \throws std::invalid_argument for invalid pseudonym
  [[nodiscard]] std::string text() const;

  /// \throws std::invalid_argument for invalid representation
  /// \warning Lazy unpacking may occur, meaning that any other method may also throw if the serialization was invalid
  [[nodiscard]] static LocalPseudonym FromPacked(std::string_view packed);
  /// \throws std::invalid_argument for invalid pseudonym
  [[nodiscard]] std::string_view pack() const;

  /// \throws std::invalid_argument for invalid pseudonym
  [[nodiscard]] class EncryptedLocalPseudonym encrypt(const ElgamalPublicKey& pk) const;

  [[nodiscard]] auto operator<=>(const LocalPseudonym& right) const = default;

  /// Ensure we have a packed representation available
  void ensurePacked() const;
  /// Ensure we have a packed and unpacked representation available
  void ensureThreadSafe() const;
};


/// Base class for encrypted pseudonyms
class EncryptedPseudonym {
  friend std::hash<EncryptedPseudonym>;

  ElgamalEncryption mEncryption;

protected:
  /// Parse printable (hex) representation from text()
  /// \throws std::invalid_argument for invalid representation
  /// \warning Lazy unpacking may occur, meaning that any other method may also throw if the serialization was invalid
  [[nodiscard]] static ElgamalEncryption FromText(std::string_view text);

  /// \throws std::invalid_argument for invalid representation
  /// \warning Lazy unpacking may occur, meaning that any other method may also throw if the serialization was invalid
  [[nodiscard]] static ElgamalEncryption FromPacked(std::string_view packed);

  /// \throws std::invalid_argument for invalid encrypted pseudonym
  [[nodiscard]] ElgamalEncryption rerandomize() const;

public:
  /// Construct invalid encrypted pseudonym
  EncryptedPseudonym() = default;

  /// \warning You normally don't need this except for serialization
  /// \throws std::invalid_argument for invalid encryption
  explicit EncryptedPseudonym(ElgamalEncryption encryption);

  /// Get inner \c ElgamalEncryption
  /// \warning You normally don't need this except for serialization
  /// \throws std::invalid_argument for invalid encryption (zero public key)
  const ElgamalEncryption& getValidElgamalEncryption() const;

  /// Get length of string returned by text()
  [[nodiscard]] static size_t TextLength();
  /// Get printable (hex) representation
  /// \throws std::invalid_argument for invalid encrypted pseudonym
  [[nodiscard]] std::string text() const;

  /// \throws std::invalid_argument for invalid encrypted pseudonym
  [[nodiscard]] std::string pack() const;

  [[nodiscard]] auto operator<=>(const EncryptedPseudonym& right) const = default;

  /// Ensure we have a packed representation available
  void ensurePacked() const;
  /// Ensure we have a packed and unpacked representation available
  void ensureThreadSafe() const;
};

namespace detail {

/// Adds functions returning derived type to \c EncryptedPseudonym
template<typename Derived>
class TypedEncryptedPseudonym : public EncryptedPseudonym {
  friend Derived;
  TypedEncryptedPseudonym() = default;
public:
  using EncryptedPseudonym::EncryptedPseudonym;

  /// \copydoc EncryptedPseudonym::FromText
  [[nodiscard]] static Derived FromText(std::string_view text) {
    return Derived(EncryptedPseudonym::FromText(text));
  }

  /// \copydoc EncryptedPseudonym::FromPacked
  [[nodiscard]] static Derived FromPacked(std::string_view packed) {
    return Derived(EncryptedPseudonym::FromPacked(packed));
  }

  /// \copydoc EncryptedPseudonym::rerandomize
  [[nodiscard]] Derived rerandomize() const /*override*/ {
    return Derived(EncryptedPseudonym::rerandomize());
  }
};

} // namespace detail

class EncryptedLocalPseudonym final : public detail::TypedEncryptedPseudonym<EncryptedLocalPseudonym> {
public:
  using TypedEncryptedPseudonym::TypedEncryptedPseudonym;

  /// \throws std::invalid_argument for invalid encrypted pseudonym
  [[nodiscard]] LocalPseudonym decrypt(const ElgamalPrivateKey& sk) const;
};

class PolymorphicPseudonym final : public detail::TypedEncryptedPseudonym<PolymorphicPseudonym> {
public:
  using TypedEncryptedPseudonym::TypedEncryptedPseudonym;

  /// \param identifier Participant identifier
  [[nodiscard]] static PolymorphicPseudonym FromIdentifier(
    const ElgamalPublicKey& masterPublicKeyPseudonyms,
    std::string_view identifier);
};

} // namespace pep

// Specialize std::hash to enable usage of pseudonyms as keys in unordered set/map
template<>
struct std::hash<pep::LocalPseudonym> {
  [[nodiscard]] size_t operator()(const pep::LocalPseudonym& p) const;
};

template<>
struct std::hash<pep::EncryptedPseudonym> {
  [[nodiscard]] size_t operator()(const pep::EncryptedPseudonym& p) const;
};

template<>
struct std::hash<pep::EncryptedLocalPseudonym> {
  [[nodiscard]] size_t operator()(const pep::EncryptedLocalPseudonym& p) const {
    return std::hash<pep::EncryptedPseudonym>{}(p);
  }
};

template<>
struct std::hash<pep::PolymorphicPseudonym> {
  [[nodiscard]] size_t operator()(const pep::PolymorphicPseudonym& p) const {
    return std::hash<pep::EncryptedPseudonym>{}(p);
  }
};
