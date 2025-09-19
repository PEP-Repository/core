#include <pep/rsk-pep/PseudonymTranslator.hpp>
#include <pep/utils/CollectionUtils.hpp>
#include <pep/utils/Random.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

using namespace pep;

namespace {

class PseudonymTranslatorTest : public ::testing::Test {
protected:
  std::vector<PseudonymTranslator> translators;
  ElgamalPrivateKey masterPrivateEncryptionKey;
  ElgamalPublicKey masterPublicEncryptionKey;

  PseudonymTranslatorTest() {
    constexpr unsigned translatorsCount{2};

    masterPrivateEncryptionKey = CurveScalar::One();
    for (unsigned i{}; i < translatorsCount; ++i) {
      const auto masterPrivateEncryptionKeyShare = CurveScalar::Random();
      masterPrivateEncryptionKey = masterPrivateEncryptionKey.mult(masterPrivateEncryptionKeyShare);
      translators.emplace_back(
          PseudonymTranslationKeys{
              .encryptionKeyFactorSecret{RandomArray<64>()},
              .pseudonymizationKeyFactorSecret{RandomArray<64>()},
              .masterPrivateEncryptionKeyShare{std::as_bytes(ToSizedSpan<32>(masterPrivateEncryptionKeyShare.pack()))},
          });
    }
    masterPublicEncryptionKey = CurvePoint::BaseMult(masterPrivateEncryptionKey);
  }

  void translateTest(const bool certified) {
    const PseudonymTranslator::Recipient userA1(1, {.reshuffle = "GroupA", .rekey = "User1"});

    std::optional<PolymorphicPseudonym> prevPolymorph;
    std::optional<LocalPseudonym> prevLocalA;
    for (unsigned i{}; i < 2; ++i) {
      const auto polymorph = PolymorphicPseudonym::FromIdentifier(masterPublicEncryptionKey, "PEP1234");
      if (const auto prev = std::exchange(prevPolymorph, polymorph)) {
        ASSERT_NE(polymorph, prev) << "PolymorphicPseudonym::FromIdentifier should be non-deterministic";
      }

      std::unique_ptr<EncryptedPseudonym> encLocalA1 = std::make_unique<PolymorphicPseudonym>(polymorph);
      bool first = true;
      for (const auto& translator : translators) {
        if (std::exchange(first, false) && certified) {
          const auto [afterStep, proof] = translator.certifiedTranslateStep(*encLocalA1, userA1);
          ASSERT_NE(afterStep, *encLocalA1);
          const auto verifiers = translator.computeTranslationProofVerifiers(userA1, masterPublicEncryptionKey);
          EXPECT_NO_THROW(translator.checkTranslationProof(*encLocalA1, afterStep, proof, verifiers));
          *encLocalA1 = afterStep;
        } else {
          const auto afterStep = translator.translateStep(*encLocalA1, userA1);
          ASSERT_NE(afterStep, *encLocalA1);
          *encLocalA1 = afterStep;
        }
      }

      ElgamalPrivateKey sk1 = CurveScalar::One();
      for (const auto& translator: translators) {
        const auto comp = translator.generateKeyComponent(userA1);
        ASSERT_NE(comp, CurveScalar::One());
        sk1 = sk1.mult(comp);
      }
      ASSERT_NE(sk1, CurveScalar::One());
      ASSERT_NE(sk1, CurveScalar{});

      const auto localA = static_cast<const EncryptedLocalPseudonym&>(*encLocalA1).decrypt(sk1);
      if (const auto prev = std::exchange(prevLocalA, localA)) {
        EXPECT_EQ(localA, prev) << "Multiple translations should yield the same local pseudonym";
      }
    }
  }
};

TEST_F(PseudonymTranslatorTest, translate) {
  translateTest(false);
}

TEST_F(PseudonymTranslatorTest, certifiedTranslate) {
  translateTest(true);
}

}
