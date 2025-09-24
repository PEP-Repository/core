#include <pep/rsk-pep/Pseudonyms.hpp>

#include <stdexcept>
#include <utility>

using namespace pep;

LocalPseudonym::LocalPseudonym(CurvePoint point0)
    : mPoint(point0) {
  (void) getValidCurvePoint();
}

const CurvePoint& LocalPseudonym::getValidCurvePoint() const {
  if (mPoint.isZero()) {
    throw std::invalid_argument("LocalPseudonym cannot have zero point");
  }
  return mPoint;
}

LocalPseudonym LocalPseudonym::Random() {
  return LocalPseudonym(CurvePoint::Random());
}

LocalPseudonym LocalPseudonym::FromText(std::string_view text) {
  return LocalPseudonym(CurvePoint::FromText(std::string(text))); //TODO? eliminate string allocation
}

size_t LocalPseudonym::TextLength() {
  return CurvePoint::TextLength();
}

std::string LocalPseudonym::text() const {
  return getValidCurvePoint().text();
}

LocalPseudonym LocalPseudonym::FromPacked(std::string_view packed) {
  return LocalPseudonym(CurvePoint(packed));
}

std::string_view LocalPseudonym::pack() const {
  return getValidCurvePoint().pack();
}

EncryptedLocalPseudonym LocalPseudonym::encrypt(const ElgamalPublicKey& pk) const {
  return EncryptedLocalPseudonym(ElgamalEncryption(pk, getValidCurvePoint()));
}

void LocalPseudonym::ensurePacked() const {
  mPoint.ensurePacked();
}

void LocalPseudonym::ensureThreadSafe() const {
  mPoint.ensureThreadSafe();
}


EncryptedPseudonym::EncryptedPseudonym(ElgamalEncryption encryption0)
    : mEncryption(encryption0) {
  (void) getValidElgamalEncryption();
}

const ElgamalEncryption& EncryptedPseudonym::getValidElgamalEncryption() const {
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
  return getValidElgamalEncryption().rerandomize();
}

size_t EncryptedPseudonym::TextLength() {
  return ElgamalEncryption::TextLength();
}

std::string EncryptedPseudonym::text() const {
  return getValidElgamalEncryption().text();
}

std::string EncryptedPseudonym::pack() const {
  return getValidElgamalEncryption().pack();
}

void EncryptedPseudonym::ensurePacked() const {
  mEncryption.ensurePacked();
}

void EncryptedPseudonym::ensureThreadSafe() const {
  mEncryption.ensureThreadSafe();
}


LocalPseudonym EncryptedLocalPseudonym::decrypt(const ElgamalPrivateKey& sk) const {
  return LocalPseudonym(getValidElgamalEncryption().decrypt(sk));
}


PolymorphicPseudonym PolymorphicPseudonym::FromIdentifier(
    const ElgamalPublicKey& masterPublicKeyPseudonyms,
    std::string_view identifier) {
  return PolymorphicPseudonym(ElgamalEncryption(masterPublicKeyPseudonyms, CurvePoint::Hash(identifier)));
}

size_t std::hash<LocalPseudonym>::operator()(const LocalPseudonym& p) const {
  return hash<CurvePoint>{}(p.mPoint);
}

size_t std::hash<EncryptedPseudonym>::operator()(
    const EncryptedPseudonym& p) const {
  return hash<ElgamalEncryption>{}(p.mEncryption);
}
