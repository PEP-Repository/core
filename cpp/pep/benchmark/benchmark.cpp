#include <benchmark/benchmark.h>

#include <boost/algorithm/hex.hpp>

#include <vector>

#include <pep/elgamal/CurvePoint.hpp>
#include <pep/elgamal/CurveScalar.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/utils/Sha.hpp>
#include <pep/accessmanager/AccessManagerSerializers.hpp>
#include <pep/storagefacility/StorageFacilitySerializers.hpp>
#include <pep/crypto/CPRNG.hpp>

namespace {
void SetBytesProcessed(benchmark::State& state, size_t bytesPerIteration)
{
  // Casts to be compatible with pre & post-v1.6.2 where IterationCount became signed
  state.SetBytesProcessed(static_cast<int64_t>(
      state.iterations() * static_cast<benchmark::IterationCount>(bytesPerIteration)));
}
}

// Silence errors about `auto _`
//NOLINTBEGIN(clang-analyzer-deadcode.DeadStores)

static void BM_CurvePointUnpack(benchmark::State& state) {
  std::string packed(boost::algorithm::unhex(std::string(
                       "b01d60504aa5f4c5bd9a7541c457661f9a789d18cb4e136e91d3c953488bd208")));
  for (auto _ : state)
    benchmark::DoNotOptimize(pep::CurvePoint(packed, true));
}
BENCHMARK(BM_CurvePointUnpack);

static void BM_CurvePointPack(benchmark::State& state) {
  // For proper measurement, we have to prevent CurvePoint from caching
  // the packed result.
  auto pt = pep::CurvePoint::Random().add(pep::CurvePoint::Random());
  for (auto _ : state)
    benchmark::DoNotOptimize(pep::CurvePoint(pt).pack());
}
BENCHMARK(BM_CurvePointPack);

static void BM_CurvePointAdd(benchmark::State& state) {
  pep::CurvePoint pt(boost::algorithm::unhex(std::string(
       "b01d60504aa5f4c5bd9a7541c457661f9a789d18cb4e136e91d3c953488bd208")));
  for (auto _ : state)
    benchmark::DoNotOptimize(pt.add(pt));
}
BENCHMARK(BM_CurvePointAdd);

static void BM_CurvePointDouble(benchmark::State& state) {
  pep::CurvePoint pt(boost::algorithm::unhex(std::string(
       "b01d60504aa5f4c5bd9a7541c457661f9a789d18cb4e136e91d3c953488bd208")));
  for (auto _ : state)
    benchmark::DoNotOptimize(pt.dbl());
}
BENCHMARK(BM_CurvePointDouble);

static void BM_ScalarMultTableCompute(benchmark::State& state) {
  pep::CurvePoint pt(boost::algorithm::unhex(std::string(
       "b01d60504aa5f4c5bd9a7541c457661f9a789d18cb4e136e91d3c953488bd208")));
  for (auto _ : state)
    benchmark::DoNotOptimize(pep::CurvePoint::ScalarMultTable(pt));
}
BENCHMARK(BM_ScalarMultTableCompute);

static void BM_ScalarMultTable(benchmark::State& state) {
  pep::CurvePoint pt(boost::algorithm::unhex(std::string(
       "b01d60504aa5f4c5bd9a7541c457661f9a789d18cb4e136e91d3c953488bd208")));
  auto scalar = pep::CurveScalar::From64Bytes("1234567890123456789012345678901234567890123456789012345678901234");
  pep::CurvePoint::ScalarMultTable table(pt);
  for (auto _ : state)
    benchmark::DoNotOptimize(table.mult(scalar));
}
BENCHMARK(BM_ScalarMultTable);

static void BM_ScalarBaseMult(benchmark::State& state) {
  auto scalar = pep::CurveScalar::From64Bytes("1234567890123456789012345678901234567890123456789012345678901234");
  for (auto _ : state)
    benchmark::DoNotOptimize(pep::CurvePoint::BaseMult(scalar));
}
BENCHMARK(BM_ScalarBaseMult);

static void BM_ScalarPublicBaseMult(benchmark::State& state) {
  auto scalar = pep::CurveScalar::From64Bytes("1234567890123456789012345678901234567890123456789012345678901234");
  // Not really fair to use fixed scalar as this is not a constant-time operation
  for (auto _ : state)
    benchmark::DoNotOptimize(pep::CurvePoint::PublicBaseMult(scalar));
}
BENCHMARK(BM_ScalarPublicBaseMult);

static void BM_ScalarMult(benchmark::State& state) {
  auto scalar = pep::CurveScalar::From64Bytes("1234567890123456789012345678901234567890123456789012345678901234");
  pep::CurvePoint pt(boost::algorithm::unhex(std::string(
       "b01d60504aa5f4c5bd9a7541c457661f9a789d18cb4e136e91d3c953488bd208")));
  for (auto _ : state)
    benchmark::DoNotOptimize(pt.mult(scalar));
}
BENCHMARK(BM_ScalarMult);

static void BM_PublicScalarMult(benchmark::State& state) {
  auto scalar = pep::CurveScalar::From64Bytes("1234567890123456789012345678901234567890123456789012345678901234");
  pep::CurvePoint pt(boost::algorithm::unhex(std::string(
       "b01d60504aa5f4c5bd9a7541c457661f9a789d18cb4e136e91d3c953488bd208")));
  // Not really fair to use fixed scalar as publicMult is not constant-time.
  for (auto _ : state)
    benchmark::DoNotOptimize(pt.publicMult(scalar));
}
BENCHMARK(BM_PublicScalarMult);

static void BM_CurvePointElligatorHash(benchmark::State& state) {
  for (auto _ : state)
    pep::CurvePoint::Hash("test string");
}
BENCHMARK(BM_CurvePointElligatorHash);

static void BM_CurveScalarInvert(benchmark::State& state) {
  auto scalar = pep::CurveScalar::From64Bytes("1234567890123456789012345678901234567890123456789012345678901234");
  for (auto _ : state)
    benchmark::DoNotOptimize(scalar.invert());
}
BENCHMARK(BM_CurveScalarInvert);

static void BM_CurveScalarMul(benchmark::State& state) {
  auto scalar = pep::CurveScalar::From64Bytes("1234567890123456789012345678901234567890123456789012345678901234");
  for (auto _ : state)
    benchmark::DoNotOptimize(scalar.mult(scalar));
}
BENCHMARK(BM_CurveScalarMul);

static void BM_CurveScalarSquare(benchmark::State& state) {
  auto scalar = pep::CurveScalar::From64Bytes("1234567890123456789012345678901234567890123456789012345678901234");
  for (auto _ : state)
    benchmark::DoNotOptimize(scalar.square());
}
BENCHMARK(BM_CurveScalarSquare);

static void BM_Sha512Short(benchmark::State& state) {
  std::string msg("Some input message ..........");
  for (auto _ : state)
    benchmark::DoNotOptimize(pep::Sha512().digest(msg));
}
BENCHMARK(BM_Sha512Short);

static void BM_Sha512Long(benchmark::State& state) {
  std::string msg;
  msg.resize(1024*1024);
  for (auto _ : state)
    benchmark::DoNotOptimize(pep::Sha512().digest(msg));
  SetBytesProcessed(state, msg.size());
}
BENCHMARK(BM_Sha512Long);

static void BM_Sha256Short(benchmark::State& state) {
  std::string msg("Some input message ..........");
  for (auto _ : state)
    benchmark::DoNotOptimize(pep::Sha256().digest(msg));
}
BENCHMARK(BM_Sha256Short);

static void BM_Sha256Long(benchmark::State& state) {
  std::string msg;
  msg.resize(1024*1024);
  for (auto _ : state)
    benchmark::DoNotOptimize(pep::Sha256().digest(msg));
  SetBytesProcessed(state, msg.size());
}
BENCHMARK(BM_Sha256Long);

static void BM_PageDecrypt(benchmark::State& state) {
  pep::DataPayloadPage page;
  std::string plaintext(1000*1000, '\0');
  pep::Metadata md;
  std::string key;
  key.resize(32);
  page.setEncrypted(plaintext, key, md);
  for (auto _ : state) {
    benchmark::DoNotOptimize(page.decrypt(key, md));
  }
  SetBytesProcessed(state, plaintext.size());
}
BENCHMARK(BM_PageDecrypt);

static void BM_PageEncrypt(benchmark::State& state) {
  pep::DataPayloadPage page;
  std::string plaintext(1000*1000, '\0');
  pep::Metadata md;
  std::string key;
  key.resize(32);
  for (auto _ : state) {
    page.setEncrypted(plaintext, key, md);
    benchmark::DoNotOptimize(page);
  }
  SetBytesProcessed(state, plaintext.size());
}
BENCHMARK(BM_PageEncrypt);

static void BM_PageSerialize(benchmark::State& state) {
  pep::DataPayloadPage page;
  std::string plaintext(1000*1000, '\0');
  pep::Metadata md;
  std::string key;
  key.resize(32);
  page.setEncrypted(plaintext, key, md);

  // We would like to write the benchmark simply as follows:
  //
  //   for (auto _ : state) {
  //     benchmark::DoNotOptimize(pep::Serialization::ToString(page);
  //   }
  //
  // This will, however, cause a copy of DataPayloadPage on every iteration,
  // which entails a separate mmap and thus significant pressure on the
  // memory subsystem.  In this benchmark we want to measure the
  // serialiation of DataPayloadPage's --- not its allocatio issues.
  // Thus we preallocate 500MB of pages, which makes the benchmark
  // code a bit dense.

  std::vector<pep::DataPayloadPage> pages;
  pages.resize(500);
  auto i = pages.size();
  for (auto _ : state) {
    if (i == pages.size()) {
      state.PauseTiming();
      for (size_t j = 0; j < pages.size(); j++)
        pages[j] = page;
      i = 0;
      state.ResumeTiming();
    }
    benchmark::DoNotOptimize(pep::Serialization::ToString(std::move(pages[i++])));
  }
  SetBytesProcessed(state, plaintext.size());
}
BENCHMARK(BM_PageSerialize);

static void BM_PageDeserialize(benchmark::State& state) {
  pep::DataPayloadPage page;
  std::string plaintext(1000*1000, '\0');
  pep::Metadata md;
  std::string key;
  key.resize(32);
  page.setEncrypted(plaintext, key, md);
  std::string serialized = pep::Serialization::ToString(page);
  for (auto _ : state) {
    benchmark::DoNotOptimize(
            pep::Serialization::FromString<pep::DataPayloadPage>(serialized));
  }
  SetBytesProcessed(state, plaintext.size());
}
BENCHMARK(BM_PageDeserialize);

static pep::EncryptionKeyRequest CreateRandomEncryptionKeyRequest() {
  pep::EncryptionKeyRequest ret;
  pep::Ticket2 ticket;
  ticket.mModes = {"read", "write"};
  for (int i = 0; i < 200; i++)
    ticket.mColumns.push_back("Column" + std::to_string(i));
  ticket.mUserGroup = "some user group";
  auto p1 = pep::LocalPseudonym::Random();
  auto p4 = pep::LocalPseudonym::Random();
  for (int i = 0; i < 600; i++) {
    auto q = pep::ElgamalPublicKey::Random();
    pep::LocalPseudonyms lp;
    lp.mAccessManager = p1.encrypt(q);
    lp.mPolymorphic = pep::PolymorphicPseudonym::FromIdentifier(q, "1234");
    lp.mStorageFacility = p4.encrypt(q);
    ticket.mPseudonyms.push_back(lp);
  }
  auto identity = pep::X509Identity::MakeSelfSigned("Benchmarker, inc.", "PepBenchmark");
  ret.mTicket2 = std::make_shared<pep::SignedTicket2>(
      ticket, identity);
  for (uint32_t i = 0; i < 1000; i++) {
    pep::KeyRequestEntry kre;
    kre.mMetadata.setTag("some tag" + std::to_string(i));
    kre.mPseudonymIndex = i;
    auto p = pep::CurvePoint::Random();
    kre.mPolymorphEncryptionKey = pep::EncryptedKey(p, p);
    ret.mEntries.push_back(std::move(kre));
  }
  return ret;
}

static void BM_KeyRequestSerialize(benchmark::State& state) {
  auto req = CreateRandomEncryptionKeyRequest();
  for (auto _ : state)
    benchmark::DoNotOptimize(pep::Serialization::ToString(req));
  SetBytesProcessed(state, pep::Serialization::ToString(req).size());
}
BENCHMARK(BM_KeyRequestSerialize);

static void BM_KeyRequestReserialize(benchmark::State& state) {
  auto req = CreateRandomEncryptionKeyRequest();
  auto packedReq = pep::Serialization::ToString(req);
  auto unpackedReq = pep::Serialization::FromString<pep::EncryptionKeyRequest>(packedReq);
  for (auto _ : state)
    benchmark::DoNotOptimize(pep::Serialization::ToString(unpackedReq));
  SetBytesProcessed(state, packedReq.size());
}
BENCHMARK(BM_KeyRequestReserialize);

static void BM_KeyRequestDeserialize(benchmark::State& state) {
  auto req = CreateRandomEncryptionKeyRequest();
  auto packedReq = pep::Serialization::ToString(req);
  for (auto _ : state)
    benchmark::DoNotOptimize(pep::Serialization::FromString<pep::EncryptionKeyRequest>(packedReq));
  SetBytesProcessed(state, packedReq.size());
}
BENCHMARK(BM_KeyRequestDeserialize);

static void BM_KeyRequestCopy(benchmark::State& state) {
  auto req = CreateRandomEncryptionKeyRequest();
  auto packedReq = pep::Serialization::ToString(req);
  for (auto _ : state)
    benchmark::DoNotOptimize(pep::EncryptionKeyRequest(req));
  SetBytesProcessed(state, packedReq.size());
}
BENCHMARK(BM_KeyRequestCopy);

const std::string SAMPLE_SHA256_DIGEST = "abcdefghijklmnopqrstuvwxyz123456"; // Digest length of 256 bits = 32 bytes

static void BM_SignDigest(benchmark::State& state) {
  pep::AsymmetricKeyPair keypair = pep::AsymmetricKeyPair::GenerateKeyPair();
  for (auto _ : state)
    benchmark::DoNotOptimize(keypair.getPrivateKey().signDigestSha256(SAMPLE_SHA256_DIGEST));
}
BENCHMARK(BM_SignDigest);

static void BM_VerifyDigest(benchmark::State& state) {
  pep::AsymmetricKeyPair keypair = pep::AsymmetricKeyPair::GenerateKeyPair();
  auto sig = keypair.getPrivateKey().signDigestSha256(SAMPLE_SHA256_DIGEST);
  for (auto _ : state)
    benchmark::DoNotOptimize(keypair.getPublicKey().verifyDigestSha256(SAMPLE_SHA256_DIGEST, sig));
}
BENCHMARK(BM_VerifyDigest);

static void BM_RandomBytes(benchmark::State& state) {
  std::string s;
  for (auto _ : state) {
    pep::RandomBytes(s, 16);
    benchmark::DoNotOptimize(&s);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()*8));
}
BENCHMARK(BM_RandomBytes);

static void BM_CPURBG(benchmark::State& state) {
  pep::CPURBG gen;
  for (auto _ : state) {
    benchmark::DoNotOptimize(gen());
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()*8));
}
BENCHMARK(BM_CPURBG);

static void BM_CPRNG(benchmark::State& state) {
  pep::CPRNG gen;
  std::array<uint8_t, 32> buffer{};
  for (auto _ : state) {
    gen(buffer);
    benchmark::DoNotOptimize(buffer);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()*32));
}
BENCHMARK(BM_CPRNG);

//NOLINTEND(clang-analyzer-deadcode.DeadStores)

BENCHMARK_MAIN();
