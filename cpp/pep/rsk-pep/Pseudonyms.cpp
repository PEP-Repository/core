#include <pep/rsk-pep/Pseudonyms.hpp>

#include <stdexcept>
#include <utility>

using namespace pep;

LocalPseudonym::LocalPseudonym(CurvePoint point0)
    : mPoint(std::move(point0)) {
  validPoint();
}

const CurvePoint& LocalPseudonym::validPoint() const {
  if (mPoint.isZero()) {
    throw std::invalid_argument("LocalPseudonym cannot have zero point");
  }
  return mPoint;
}

LocalPseudonym LocalPseudonym::Random() {
  return CurvePoint::Random();
}

LocalPseudonym LocalPseudonym::FromText(std::string_view text) {
  return CurvePoint::FromText(std::string(text)); //TODO? eliminate string allocation
}

size_t LocalPseudonym::TextLength() {
  return CurvePoint::TextLength();
}

std::string LocalPseudonym::text() const {
  return validPoint().text();
}

LocalPseudonym LocalPseudonym::FromPacked(std::string_view packed) {
  return CurvePoint(packed);
}

std::string_view LocalPseudonym::pack() const {
  return validPoint().pack();
}

EncryptedLocalPseudonym LocalPseudonym::encrypt(const ElgamalPublicKey& pk) const {
  return ElgamalEncryption(pk, validPoint());
}

void LocalPseudonym::ensurePacked() const {
  mPoint.ensurePacked();
}

void LocalPseudonym::ensureThreadSafe() const {
  mPoint.ensureThreadSafe();
}


EncryptedPseudonym::EncryptedPseudonym(ElgamalEncryption encryption0)
    : mEncryption(std::move(encryption0)) {
  validEncryption();
}

const ElgamalEncryption& EncryptedPseudonym::validEncryption() const {
  if (mEncryption.getPublicKey().isZero()) { // See #500 (check used to be in Transcryptor::handleTranscryptorRequest)
    throw std::invalid_argument("EncryptedPseudonym cannot have zero public key");
  }
  return mEncryption;
}

ElgamalEncryption EncryptedPseudonym::FromText(std::string_view text) {
  return ElgamalEncryption::FromText(std::string(text));
}

ElgamalEncryption EncryptedPseudonym::FromPacked(std::string_view packed) {
  return ElgamalEncryption::FromPacked(packed);
}

ElgamalEncryption EncryptedPseudonym::rerandomize() const {
  return validEncryption().rerandomize();
}

size_t EncryptedPseudonym::TextLength() {
  return ElgamalEncryption::TextLength();
}

std::string EncryptedPseudonym::text() const {
  return validEncryption().text();
}

std::string EncryptedPseudonym::pack() const {
  return validEncryption().pack();
}

void EncryptedPseudonym::ensurePacked() const {
  mEncryption.ensurePacked();
}

void EncryptedPseudonym::ensureThreadSafe() const {
  mEncryption.ensureThreadSafe();
}


LocalPseudonym EncryptedLocalPseudonym::decrypt(const ElgamalPrivateKey& sk) const {
  return validEncryption().decrypt(sk);
}


PolymorphicPseudonym PolymorphicPseudonym::FromIdentifier(
    const ElgamalPublicKey& masterPublicKeyPseudonyms,
    std::string_view identifier) {
  return ElgamalEncryption(masterPublicKeyPseudonyms, CurvePoint::Hash(identifier));
}


void PropertySerializer<LocalPseudonym>::write(boost::property_tree::ptree& destination, const LocalPseudonym& value) const {
  SerializeProperties(destination, value.text());
}

LocalPseudonym PropertySerializer<LocalPseudonym>::read(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const {
  return LocalPseudonym::FromText(DeserializeProperties<std::string>(source, transform));
}

void PropertySerializer<EncryptedLocalPseudonym>::write(boost::property_tree::ptree& destination, const EncryptedLocalPseudonym& value) const {
  SerializeProperties(destination, value.text());
}

EncryptedLocalPseudonym PropertySerializer<EncryptedLocalPseudonym>::read(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const {
  return EncryptedLocalPseudonym::FromText(DeserializeProperties<std::string>(source, transform));
}

void PropertySerializer<PolymorphicPseudonym>::write(boost::property_tree::ptree& destination, const PolymorphicPseudonym& value) const {
  SerializeProperties(destination, value.text());
}

PolymorphicPseudonym PropertySerializer<PolymorphicPseudonym>::read(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const {
  return PolymorphicPseudonym::FromText(DeserializeProperties<std::string>(source, transform));
}


size_t std::hash<LocalPseudonym>::operator()(const LocalPseudonym& p) const {
  return hash<CurvePoint>{}(p.mPoint);
}

size_t std::hash<EncryptedPseudonym>::operator()(
    const EncryptedPseudonym& p) const {
  return hash<ElgamalEncryption>{}(p.mEncryption);
}
