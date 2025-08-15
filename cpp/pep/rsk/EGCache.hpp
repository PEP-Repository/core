#pragma once

#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/crypto/CPRNG.hpp>
namespace pep {

// Caches operations on ElgamalEncryption such as RSK.
// All operations are thread safe.
class EGCache {
public:
  static EGCache& get();

  // Caching version of eg.RSK(z, k) --- faster if called ~20 times with
  // same (eg.y, k).
  virtual ElgamalEncryption RSK(
    ElgamalEncryption eg,
    const CurveScalar& z,
    ElgamalTranslationKey k,
    CPRNG* rng=nullptr
  ) = 0;

  // Caching version of eg.rerandomize().rekey(k) --- faster if called
  // same (eg.y, k).
  virtual ElgamalEncryption RK(
    ElgamalEncryption eg,
    ElgamalTranslationKey k,
    CPRNG* rng=nullptr
  ) = 0;

  // Caching version of eg.rerandomize() --- faster if called
  // same eg.y.
  virtual ElgamalEncryption rerandomize(
    ElgamalEncryption eg,
    CPRNG* rng=nullptr
  ) = 0;

  // Caching version of std::make_shared<CurvePoint::ScalarMultTable>(b).
  // Returns nullptr when cache is disabled.
  virtual std::shared_ptr<CurvePoint::ScalarMultTable>
  scalarMultTable(const CurvePoint& b) = 0;

  // Since the EGCache will be called from many different threads,
  // it does not send its metrics directly to a prometheus registry.
  // Instead the metrics of the EGCache can be pulled with the following method.
  struct Metrics {
    struct OfCache {
      uint64_t generation = 0; // = # entries added = # cache misses
      uint64_t useCount = 0; // = # of requests
    };

    OfCache rsk;
    OfCache table;
  };

  virtual Metrics getMetrics() = 0;

  virtual ~EGCache() = default;
};


}
