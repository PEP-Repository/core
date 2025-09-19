// This application was made to generate some testcases for pseudonym RSK operations to check compatibility with libPEP

#include <pep/application/Application.hpp>
#include <pep/morphing/RepoRecipient.hpp>
#include <pep/rsk-pep/PseudonymTranslator.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/rsk/RskTranslator.hpp>
#include <pep/utils/CollectionUtils.hpp>
#include <pep/utils/Random.hpp>

#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <boost/algorithm/hex.hpp>

using namespace pep;

namespace {

std::string ToHex(std::span<const std::byte> bytes) { return boost::algorithm::hex(std::string(SpanToString(bytes))); }

SkRecipient SomeRecipient(unsigned index) {
  auto facilityType = static_cast<FacilityType>(index % 5 + 1);
  if (facilityType == FacilityType::User) {
    return SkRecipient(
        static_cast<RecipientBase::Type>(facilityType),
        {
            .reshuffle = "Dummy user certificate " + std::to_string(index),
            .rekey = "User group " + std::to_string(index),
        });
  }
  else { return RecipientForServer(facilityType); }
}

void GenerateKeyFactorTestcases(std::ostream& out, const unsigned count) {
  out << "==== Key factor & component testcases ====\n\n";

  for (unsigned i{}; i < count; ++i) {
    RskTranslator::KeyDomainType domain = i > count / 2 ? 2 : 1;
    KeyFactorSecret rekey{RandomArray<64>()};

    RskTranslator rsk({
        .domain = domain,
        .reshuffle{},
        .rekey = rekey,
    });

    out << "Domain: " << unsigned{domain} << '\n';
    out << "Key factor secret (HMAC key): " << ToHex(rekey.hmacKey()) << '\n';

    auto facilityType = static_cast<FacilityType>(i % 5 + 1);
    RekeyRecipient recipient(static_cast<const RekeyRecipient&>(SomeRecipient(i)));

    out << "Recipient:\n"
        << "  FacilityType: " << static_cast<unsigned>(facilityType) << '\n'
        << "  Payload: " << recipient.payload() << '\n';

    auto keyFactor = rsk.generateKeyFactor(recipient);
    out << ">> Result Key factor (CurveScalar): " << keyFactor.text() << '\n';

    auto masterPrivateEncryptionKeyShare = CurveScalar::Random();
    out << "Master private encryption key share (CurveScalar): " << masterPrivateEncryptionKeyShare.text() << '\n';
    auto keyComponent = rsk.generateKeyComponent(keyFactor, masterPrivateEncryptionKeyShare);
    out << ">> Result Key component (CurveScalar): " << keyComponent.text() << '\n';

    out << '\n';
  }
  out << '\n';
}

void GeneratePseudonymTestcases(std::ostream& out, const unsigned count) {
  out << "==== Pseudonym translation testcases ====\n\n";

  constexpr unsigned translatorsCount{2};

  // Similar to PseudonymTranslator.test
  std::vector<PseudonymTranslator> translators;
  ElgamalPrivateKey masterPrivateEncryptionKey = CurveScalar::One();
  for (unsigned i{}; i < translatorsCount; ++i) {
    const auto masterPrivateEncryptionKeyShare = CurveScalar::Random();
    masterPrivateEncryptionKey = masterPrivateEncryptionKey.mult(masterPrivateEncryptionKeyShare);
    PseudonymTranslationKeys translationKeys{
        .encryptionKeyFactorSecret{RandomArray<64>()},
        .pseudonymizationKeyFactorSecret{RandomArray<64>()},
        .masterPrivateEncryptionKeyShare{std::as_bytes(ToSizedSpan<32>(masterPrivateEncryptionKeyShare.pack()))},
    };
    translators.emplace_back(translationKeys);

    out << "Transcryptor #" << std::to_string(i) << ":\n";
    out << "  Encryption key factor secret (HMAC key): " << ToHex(translationKeys.encryptionKeyFactorSecret.hmacKey())
        << '\n';
    out << "  Pseudonymization key factor secret (HMAC key): "
        << ToHex(translationKeys.pseudonymizationKeyFactorSecret.hmacKey()) << '\n';
    out << "  Master private encryption key share (CurveScalar): "
        << ToHex(translationKeys.masterPrivateEncryptionKeyShare.curveScalar()) << '\n';
  }
  out << '\n';

  out << "Master private encryption key (CurveScalar, product of shares): " << masterPrivateEncryptionKey.text()
      << '\n';
  ElgamalPublicKey masterPublicEncryptionKey = CurvePoint::BaseMult(masterPrivateEncryptionKey);
  out << "Master public encryption key (CurvePoint): " << masterPrivateEncryptionKey.text() << '\n';

  out << "\nTestcases:\n\n";

  for (unsigned i{}; i < count; ++i) {
    auto participantId = "PEP" + std::to_string(i);
    out << "Participant ID: " << participantId << '\n';
    auto polymorph = PolymorphicPseudonym::FromIdentifier(masterPublicEncryptionKey, participantId);
    out << ">> Polymorphic pseudonym (ElgamalEncryption): " << polymorph.text() << '\n';

    PseudonymTranslator::Recipient recipient = SomeRecipient(i);
    out << "Recipient:\n"
        << "  FacilityType: " << unsigned{recipient.type()} << '\n'
        << "  Payload ReShuffle: " << static_cast<const ReshuffleRecipient&>(recipient).payload() << '\n'
        << "  Payload ReKey: " << static_cast<const RekeyRecipient&>(recipient).payload() << '\n';

    std::unique_ptr<EncryptedPseudonym> encLocal = std::make_unique<PolymorphicPseudonym>(polymorph);
    bool first = true;
    for (unsigned transcryptorNum{}; const auto& translator : translators) {
      out << ">>Translate step @ transcryptor #" << transcryptorNum << ":\n";
      if (std::exchange(first, false)) {
        const auto [afterStep, proof] = translator.certifiedTranslateStep(*encLocal, recipient);
        out << "  Encrypted pseudonym: " << afterStep.text() << '\n';
        out << "  Proof:\n"
            << "    RY: " << proof.mRY.text() << '\n'
            << "    RB: " << proof.mRB.text() << '\n'
            << "    RP:\n"
            << "      CB: " << proof.mRP.mCB.text() << '\n'
            << "      CM: " << proof.mRP.mCM.text() << '\n'
            << "      S (CurveScalar): " << proof.mRP.mS.text() << '\n'
            << "    BP:\n"
            << "      CB: " << proof.mBP.mCB.text() << '\n'
            << "      CM: " << proof.mBP.mCM.text() << '\n'
            << "      S (CurveScalar): " << proof.mBP.mS.text() << '\n'
            << "    CP:\n"
            << "      CB: " << proof.mCP.mCB.text() << '\n'
            << "      CM: " << proof.mCP.mCM.text() << '\n'
            << "      S (CurveScalar): " << proof.mCP.mS.text() << '\n';
        *encLocal = afterStep;
      }
      else {
        const auto afterStep = translator.translateStep(*encLocal, recipient);
        out << "  Encrypted pseudonym: " << afterStep.text() << '\n';
        *encLocal = afterStep;
      }
      ++transcryptorNum;
    }

    out << ">>User key components:\n";
    ElgamalPrivateKey keyRecipient = CurveScalar::One();
    for (unsigned transcryptorNum{}; const auto& translator : translators) {
      const auto comp = translator.generateKeyComponent(recipient);
      out << "  Key component @ transcryptor #" << transcryptorNum << " (CurveScalar): " << comp.text() << '\n';
      keyRecipient = keyRecipient.mult(comp);
      ++transcryptorNum;
    }
    out << ">>User private key (product of components): " << keyRecipient.text() << '\n';

    out << ">>Decrypted local pseudonym: "
        << static_cast<const EncryptedLocalPseudonym&>(*encLocal).decrypt(keyRecipient).text() << '\n';

    out << '\n';
  }
  out << '\n';
}

class RskPepTestcasesApplication : public Application {
  std::string getDescription() const override { return "Generates RSK/PEP testcases"; }

  commandline::Parameters getSupportedParameters() const override {
    return Application::getSupportedParameters()
        + commandline::Parameter("count", "Number of testcases").value(commandline::Value<unsigned>().defaultsTo(20));
  }

  int execute() override {
    const auto& params = getParameterValues();
    auto numTestcases = params.get<unsigned>("count");
    GenerateKeyFactorTestcases(std::cout, numTestcases);
    GeneratePseudonymTestcases(std::cout, numTestcases);

    return EXIT_SUCCESS;
  }
};

} // namespace

PEP_DEFINE_MAIN_FUNCTION(RskPepTestcasesApplication)
