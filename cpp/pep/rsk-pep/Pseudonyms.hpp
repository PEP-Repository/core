#pragma once

#include <pep/elgamal/CurvePoint.hpp>
#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/utils/PropertySerializer.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

namespace pep {

class LocalPseudonym final {
  friend class EncryptedLocalPseudonym;
  friend std::hash<LocalPseudonym>;

  CurvePoint mPoint;
  /// \throws std::invalid_argument for invalid point
  LocalPseudonym(CurvePoint point);

  /// Check if \a mPoint is nonzero
  /// \throws std::invalid_argument for invalid point
  const CurvePoint& validPoint() const;

public:
  /// Construct invalid pseudonym
  LocalPseudonym() = default;

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
  friend class PseudonymTranslator;
protected:
  ElgamalEncryption mEncryption;
  /// \throws std::invalid_argument for invalid encryption
  EncryptedPseudonym(ElgamalEncryption encryption);

  /// Construct invalid encrypted pseudonym
  EncryptedPseudonym() = default;

  /// Check if \c mEncryption.getPublicKey() is nonzero
  /// \throws std::invalid_argument for invalid encryption
  const ElgamalEncryption& validEncryption() const;

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
public:
  using EncryptedPseudonym::EncryptedPseudonym;

  /// \copydoc EncryptedPseudonym::FromText
  [[nodiscard]] static Derived FromText(std::string_view text) {
    return EncryptedPseudonym::FromText(text);
  }

  /// \copydoc EncryptedPseudonym::FromPacked
  [[nodiscard]] static Derived FromPacked(std::string_view packed) {
    return EncryptedPseudonym::FromPacked(packed);
  }

  /// \copydoc EncryptedPseudonym::rerandomize
  [[nodiscard]] Derived rerandomize() const /*override*/ {
    return EncryptedPseudonym::rerandomize();
  }
};

} // namespace detail

class EncryptedLocalPseudonym final : public detail::TypedEncryptedPseudonym<EncryptedLocalPseudonym> {
  friend LocalPseudonym;
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


template <>
class PropertySerializer<LocalPseudonym> final : public PropertySerializerByValue<LocalPseudonym> {
public:
  void write(boost::property_tree::ptree& destination, const LocalPseudonym& value) const override;
  LocalPseudonym read(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const override;
};

template <>
class PropertySerializer<EncryptedLocalPseudonym> final : public PropertySerializerByValue<EncryptedLocalPseudonym> {
public:
  void write(boost::property_tree::ptree& destination, const EncryptedLocalPseudonym& value) const override;
  EncryptedLocalPseudonym read(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const override;
};

template <>
class PropertySerializer<PolymorphicPseudonym> final : public PropertySerializerByValue<PolymorphicPseudonym> {
public:
  void write(boost::property_tree::ptree& destination, const PolymorphicPseudonym& value) const override;
  PolymorphicPseudonym read(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const override;
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
