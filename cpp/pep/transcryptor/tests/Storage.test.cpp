#include <gtest/gtest.h>

#include <pep/transcryptor/Storage.hpp>

#include <pep/morphing/RepoRecipient.hpp>
#include <pep/rsk-pep/PseudonymTranslator.hpp>
#include <pep/utils/Random.hpp>

using namespace pep;

namespace {

class TranscryptorStorageTest : public ::testing::Test {
public:
  static std::optional<TranscryptorStorage> storage;
  const std::filesystem::path databasePath{":memory:"};

  // Create a new TranscryptorStorage with a clean database
  void SetUp() override {
    std::filesystem::remove(databasePath);
    storage.emplace(databasePath);
  }

  void TearDown() override {
    std::filesystem::remove(databasePath);
  }
};

std::optional<TranscryptorStorage> TranscryptorStorageTest::storage;

#include <pep/transcryptor/tests/TestCertificates.inc>

TEST_F(TranscryptorStorageTest, userVerifiers) {
  ASSERT_EQ(storage->getUserVerifiers(ResearchAssessorCert1), std::nullopt)
    << "Database should start out with no verifiers";
  ASSERT_EQ(storage->getUserVerifiers(ResearchAssessorCert2), std::nullopt);

  // Pretend this is the AccessManager sending us verifiers
  const ElgamalPublicKey masterPublicEncryptionKey = CurvePoint::Random();
  const PseudonymTranslator amTranslator({
    .encryptionKeyFactorSecret{RandomArray<64>()},
    .pseudonymizationKeyFactorSecret{RandomArray<64>()},
    .masterPrivateEncryptionKeyShare{std::as_bytes(ToSizedSpan<32>(CurveScalar::Random().pack()))},
  });

  const auto verifiersResearchAssessor1 =
    amTranslator.computeTranslationProofVerifiers(RecipientForCertificate(ResearchAssessorCert1), masterPublicEncryptionKey);
  EXPECT_NO_THROW(storage->checkAndStoreUserVerifiers(ResearchAssessorCert1, verifiersResearchAssessor1));
  // Idempotent
  EXPECT_NO_THROW(storage->checkAndStoreUserVerifiers(ResearchAssessorCert1, verifiersResearchAssessor1))
    << "Storing same verifiers twice should do nothing";
  EXPECT_EQ(storage->getUserVerifiers(ResearchAssessorCert1), verifiersResearchAssessor1)
    << "Just-stored verifiers should be retrievable";

  const SkRecipient researchAssessor2WrongReshuffle{
    static_cast<RecipientBase::Type>(EnrolledParty::User),
    {
      .reshuffle = "This is wrong",
      .rekey = std::string(static_cast<const RekeyRecipient&>(RecipientForCertificate(ResearchAssessorCert2)).payload()),
    },
  };
  const auto verifiersResearchAssessor2WrongReshuffle =
    amTranslator.computeTranslationProofVerifiers(researchAssessor2WrongReshuffle, masterPublicEncryptionKey);
  // Try to store verifiers for new session 2 with reshuffle factor different from session 1 of same domain
  EXPECT_THROW(storage->checkAndStoreUserVerifiers(ResearchAssessorCert2, verifiersResearchAssessor2WrongReshuffle),
    std::runtime_error) << "Storing verifiers for new session with wrong domain verifiers should fail";
  EXPECT_EQ(storage->getUserVerifiers(ResearchAssessorCert2), std::nullopt)
    << "Incorrect verifiers should not be stored";
  EXPECT_EQ(storage->getUserVerifiers(ResearchAssessorCert1), verifiersResearchAssessor1)
    << "Correct verifiers should not be overwritten";

  const auto verifiersResearchAssessor2 =
    amTranslator.computeTranslationProofVerifiers(RecipientForCertificate(ResearchAssessorCert2), masterPublicEncryptionKey);
  // Try to store verifiers from session 2 at session 1
  EXPECT_THROW(storage->checkAndStoreUserVerifiers(ResearchAssessorCert1, verifiersResearchAssessor2),
    std::runtime_error) << "Storing different verifiers from wrong session should fail";
  EXPECT_EQ(storage->getUserVerifiers(ResearchAssessorCert1), verifiersResearchAssessor1)
    << "Correct verifiers should not be overwritten";

  EXPECT_NO_THROW(storage->checkAndStoreUserVerifiers(ResearchAssessorCert2, verifiersResearchAssessor2))
    << "Storing verifiers for new session with existing domain should succeed";
  EXPECT_EQ(storage->getUserVerifiers(ResearchAssessorCert2), verifiersResearchAssessor2)
    << "Just-stored verifiers of new session should be retrievable";
}

}
