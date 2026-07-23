#include <pep/rsk/EGCache.hpp>

#include <pep/utils/Timestamp.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/Singleton.hpp>

#include <boost/core/noncopyable.hpp>
#include <boost/functional/hash.hpp>

#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <atomic>

// There are currently two caches:
//
//    - the precomputed scalar multiples tables cache (~30KB per entry)
//    - RSK cache (<1KB per entry)
//
// Entries in the RSK cache contain shared_ptr to tables in the tables cache.
//
// The caches are protected by read/write-locks (shared_mutex), that is,
// multiple readers (shared_lock), but only wone writer (unique_lock).
// Each cache contains a generation counter which is incremented on
// every new entry.
// If an entry is used, the current generation counter is recorded to the
// entry.*  If a cache grows too big, the least recently used entries are
// pruned.
//
// If a cache is pruned twice in a short period, the cache is disabled
// altogether to prevent denail of service.
//
// * We do not protect the generation counters of entries with additional
// locks or a memory barriers.
// This means that two updates of the generation counter of an entry are
// synchronised only when the cache has been modified inbetween.
// But if two updates happen between consecutive writes to the cache,
// that is, within one generation, they update the counter to the same
// value, and so synchronisation is not required anyways.
// (Besides, the value of the entry's generation counter is used only
// during pruning, when the write lock is held.)
//
// We did, however, make the entries' generation counters atomic, because
// otherwise the lack of synchronisation between their updates
// constitutes a data race and thus undefined behaviour,
//   https://en.cppreference.com/w/cpp/language/memory_model .
// This is not just a formality since compilers can (and do) optimise
// on the assumption that undefined behaviour does not occur, see e.g.
//   https://www.usenix.org/legacy/events/hotpar11/tech/final%5ffiles/Boehm.pdf
//
// By default operations on atomics are synchronised by memory barriers,
// but this can be disabled by passing std::memory_model_relaxed to the
// "load" and "store" methods.

using namespace std::literals;

namespace pep {

namespace {

const std::string LogTag("EGCache");

class EGCacheImp
  : public StaticSingleton<EGCacheImp>, public EGCache,
    private boost::noncopyable
{
private:
  // The RSK and table cache share the following pattern.
  template<typename Key, typename Value, typename Options,
    typename KeyHash=std::hash<Key>>
  class Cache {
    // The 'Options' type should have the following static members:
    static_assert(std::is_same_v<decltype(Options::PrunedSize), const size_t>);
    static_assert(std::is_same_v<decltype(Options::MaxSize), const size_t>);
    static_assert(std::is_same_v<decltype(Options::Name), const char* const>);

    // 'Value' should have a constructor with signature
    //    Value(EGCacheImp* egcache, Key&& key)
    // The EGCache is passed along, because RekeyValue needs to access
    // the table cache.
    static_assert(std::is_constructible_v<Value, EGCacheImp*, const Key&>);

    // 'Value' should have an explicit operator bool that indicates whether
    // it was constructed successfully.
    // TODO: static_assert this.

    class Entry {
    private:
      Value value_;
      std::atomic_uint64_t lastUse_;

    public:
      Entry(EGCacheImp* egcache, const Key& key, uint64_t generation)
        : value_(egcache, key), lastUse_(generation) {}

      inline uint64_t getLastUse() const {
        return lastUse_.load(std::memory_order_relaxed);
      }

      inline void updateLastUse(uint64_t generation) {
        lastUse_.store(generation, std::memory_order_relaxed);
      }

      inline Value& getValue() {
        return value_;
      }

      inline explicit operator bool() const {
        return value_ ? true : false;
      }
    };

    std::shared_mutex mux_;
    std::unordered_map<Key, Entry, KeyHash> data_;
    uint64_t generation_ = 0;
    std::atomic_uint64_t useCount_ = 0; // for metrics
    steady_seconds
        lastPrune_ = TimeNow<steady_seconds>(),
        disabledAt_ = TimeNow<steady_seconds>();
    bool enabled_ = true;

    using iterator = typename decltype(data_)::iterator;

    inline void incrementUseCount() {
      useCount_.fetch_add(1, std::memory_order_relaxed);
    }

    inline uint64_t getUseCount() {
      return useCount_.load(std::memory_order_relaxed);
    }

    inline void disableUnderUniqueLock() {
      assert(enabled_);
      enabled_ = false;
      disabledAt_ = TimeNow<steady_seconds>();
    }

    // if the cache is disabled, but the prune has cooled down,
    // the cache is said to be enablable.
    inline bool enabledOrEnablableUnderSharedLock() {
      if (enabled_)
        return true;
      return TimeNow<steady_seconds>() - disabledAt_ >= Options::ReEnableTime;
    }

    inline bool pruneCooledDownUnderSharedLock() {
      return TimeNow<steady_seconds>() - lastPrune_ >= Options::PruneCooldown;
    }

    // prune cache, which might disable it
    void pruneUnderUniqueLock() {
      assert(enabled_);
      if (!this->pruneCooledDownUnderSharedLock()) {
        PEP_LOG(LogTag, Severity::Warning)
          << Options::Name
          << " cache overflows a second time in a short while. "
          << "Disabling " << Options::Name
          << " cache to mitigate potential DoS.";
          this->disableUnderUniqueLock();
          return;
      }

      size_t toEvict = data_.size() - Options::PrunedSize;
      std::vector<std::pair<uint64_t, Key>> entries;
      entries.reserve(data_.size());
      for (const auto& pair : data_)
        entries.emplace_back(pair.second.getLastUse(), pair.first);
      std::sort(entries.begin(), entries.end(),
          [](auto& a, auto& b) { return a.first < b.first; });
      for (size_t i = 0; i < toEvict; i++) {
        data_.erase(entries[i].second);
      }

      PEP_LOG(LogTag, Severity::Info) << "Pruned " << Options::Name << " cache down to "
        << data_.size();
      lastPrune_ = TimeNow<steady_seconds>();
    }

    // adds entry associated with the given key, and returns
    // an iterator to it.
    std::optional<iterator> cacheUnderUniqueLock(
        EGCacheImp* egcache, const Key& key) {

      assert(data_.find(key) == data_.end());
      assert(enabled_);

      if (data_.size() >= Options::MaxSize) {
        // since entries are added one by one:
        assert(data_.size()==Options::MaxSize);
        this->pruneUnderUniqueLock();

        if (!enabled_)
          return std::nullopt;
      }


      auto [it, inserted] = data_.emplace(
          std::piecewise_construct,
          std::make_tuple(key),
          std::make_tuple(egcache, key, ++generation_));

      assert(inserted);

      if (!it->second) {
        // Creation of the entry failed.  The creation of an RSK entry might
        // fail, for example, when the table cache is disabled.
        PEP_LOG(LogTag, Severity::Warning)
          << "Failed to add entry to " << Options::Name << " cache; "
            << "disabling. ";
        data_.erase(it);
        this->disableUnderUniqueLock();
        return std::nullopt;
      }

      PEP_LOG(LogTag, Severity::Debug)
        << "Entry added to " << Options::Name << " cache.  size: "
        << data_.size()
          << "; generation: " << generation_;

      return it;
    }

    // get cache entry associated with key, if it exists
    std::optional<iterator> getUnderSharedLock(const Key& key) {
      auto it = data_.find(key);
      if (it != data_.end()) {
        it->second.updateLastUse(generation_);
        this->incrementUseCount();
        return it;
      }
      return std::nullopt;
    }

  public:
    // get cache entry associated with key, which is created if needed,
    // unless the cache is disabled.
    std::optional<iterator> get(EGCacheImp* egcache, const Key& key) {
      {
        auto readLock = std::shared_lock(mux_);

        auto it = this->getUnderSharedLock(key);

        if (it)
          return it;

        if (!this->enabledOrEnablableUnderSharedLock())
          return std::nullopt;

        // key is not present, so we need to obtain a write lock to add it
      }

      auto writeLock = std::unique_lock(mux_);

      // the key might have been added in the meantime
      auto it = this->getUnderSharedLock(key);

      if (it)
        return it;

      // the cache might have been disabled in the meantime.
      if (!this->enabledOrEnablableUnderSharedLock())
        return std::nullopt;

      // enable cache, if necessary
      if (!enabled_) {
        PEP_LOG(LogTag, Severity::Warning)
          << "Re-enabling " << Options::Name
          << " cache.";
        enabled_ = true;
      }

      return this->cacheUnderUniqueLock(egcache, key);
    }

    void fillMetrics(EGCache::Metrics::OfCache& metrics) {
      // Acquire unique lock to force all relaxed atomic
      // operations to materialize.
      // (A shared_lock would have suffices as well, if we do not care
      //  about accuracy.)
      auto uniqueLock = std::unique_lock(mux_);

      metrics.generation = generation_;
      metrics.useCount = this->getUseCount();
    }
  };


  // RSK Cache
  //
  // Caches 1/k, k*y and (reference to) scalar multiplication table of y.
  // Used to speed up the ElgamalEncryption::rsk operation and
  // ElgamalEncryption::rekey operations.
  struct RekeyKey {
    RekeyKey(const CurveScalar& k, const CurvePoint& y)
      : k(k), y(y) { }
    bool operator== (const RekeyKey& p) const;

    CurveScalar k;
    CurvePoint y;

    struct hash { size_t operator()(const pep::EGCacheImp::RekeyKey& k) const; };
  };

  struct RekeyValue {
    RekeyValue(EGCacheImp* egcache, const RekeyKey& key);

    ElgamalEncryption rsk(
            const CurvePoint& b, const CurvePoint& c,
            const ElgamalTranslationKey& reshuffle) const;
    ElgamalEncryption rk(const CurvePoint& b, const CurvePoint& c) const;

    CurveScalar kInv;
    CurvePoint kY;
    std::shared_ptr<CurvePoint::ScalarMultTable> yTable;

    // returns whether the value was successfully initialized
    inline explicit operator bool() const {
      return yTable ? true : false;
    }
  };

  struct RSKOptions {
    static constexpr size_t PrunedSize = 250;
    static constexpr size_t MaxSize = 500;
    static constexpr const char *Name = "RSK";
    static constexpr auto PruneCooldown = 15min;
    static constexpr auto ReEnableTime = 1h;
  };

  Cache<RekeyKey, RekeyValue, RSKOptions, RekeyKey::hash> rskCache_;

  // Scalar multiplication (table) cache
  struct TableValue {
    TableValue(EGCacheImp* egcache, const CurvePoint& p);
    std::shared_ptr<CurvePoint::ScalarMultTable> table_;

    // creation of a TableValue, unlike RSKValue, always succeeds
    inline explicit operator bool() const { return true; }
  };

  struct TableOptions {
    static constexpr size_t PrunedSize = 750;
    static constexpr size_t MaxSize = 1000;
    static constexpr const char *Name = "Table";
    static constexpr auto PruneCooldown = 15min;
    static constexpr auto ReEnableTime = 1h;
  };

  static_assert(TableOptions::PrunedSize > RSKOptions::MaxSize,
      "Every RSK cache entry may block a table cache entry");

  Cache<CurvePoint, TableValue, TableOptions> tableCache_;

 public:
  ElgamalEncryption rsk(
    const ElgamalEncryption& eg,
    const CurveScalar& reshuffle,
    const ElgamalTranslationKey& rekey
  ) override;

  ElgamalEncryption rk(
    const ElgamalEncryption& eg,
    const ElgamalTranslationKey& rekey
  ) override;

  ElgamalEncryption rerandomize(
    const ElgamalEncryption& eg
  ) override;

  std::shared_ptr<CurvePoint::ScalarMultTable>
  scalarMultTable(const CurvePoint& b) override;

  EGCache::Metrics getMetrics() override;

  using StaticSingleton<EGCacheImp>::Instance;
};

}

EGCache& EGCache::get() {
  return EGCacheImp::Instance();
}


std::shared_ptr<CurvePoint::ScalarMultTable>
EGCacheImp::scalarMultTable(const CurvePoint& b) {

  auto optional_it = tableCache_.get(this, b);

  if (!optional_it)
    return nullptr;

  return (*optional_it)->second.getValue().table_;
}

ElgamalEncryption EGCacheImp::rsk(
    const ElgamalEncryption& eg,
    const CurveScalar& reshuffle,
    const ElgamalTranslationKey& rekey) {

  auto key = RekeyKey(rekey, eg.publicKey);

  auto opt_it = rskCache_.get(this, key);

  if (!opt_it) {
    // fall back to uncached rsk
    return ElgamalEncryption(eg.b, eg.c, key.y)
      .rerandomize()
      .reshuffleRekey(reshuffle, key.k);
  }

  return (*opt_it)->second.getValue().rsk(eg.b, eg.c, reshuffle);
}

ElgamalEncryption EGCacheImp::rk(
    const ElgamalEncryption& eg,
    const ElgamalTranslationKey& rekey) {

  auto key = RekeyKey(rekey, eg.publicKey);

  auto opt_it = rskCache_.get(this, key);

  if (!opt_it) {
    // fall back to uncached rk
    return ElgamalEncryption(eg.b, eg.c, key.y)
      .rerandomize()
      .rekey(key.k);
  }

  return (*opt_it)->second.getValue().rk(eg.b, eg.c);
}

ElgamalEncryption EGCacheImp::rerandomize(
    const ElgamalEncryption& eg) {

  auto table = scalarMultTable(eg.publicKey);
  ElgamalEncryption ret;

  if (table == nullptr)
    return eg.rerandomize();

  auto r = CurveScalar::Random();
  ret.b = eg.b + (r * CurvePoint::Base);
  ret.c = eg.c + table->mult(r);
  ret.publicKey = eg.publicKey;
  return ret;
}

bool EGCacheImp::RekeyKey::operator== (const EGCacheImp::RekeyKey& k) const {
  return k.k == this->k && k.y == this->y;
}

ElgamalEncryption EGCacheImp::RekeyValue::rk(
    const CurvePoint& b,
    const CurvePoint& c) const {
  const auto r = CurveScalar::Random();
  return {
    kInv * (b + (r * CurvePoint::Base)),
    c + yTable->mult(r),
    kY,
  };
}

ElgamalEncryption EGCacheImp::RekeyValue::rsk(
    const CurvePoint& b,
    const CurvePoint& c,
    const ElgamalTranslationKey& reshuffle) const {
  const auto r = CurveScalar::Random();
  return {
    reshuffle * kInv * (b + (r * CurvePoint::Base)),
    reshuffle * (c + yTable->mult(r)),
    kY,
  };
}

EGCacheImp::RekeyValue::RekeyValue(
    EGCacheImp* egcache, const EGCacheImp::RekeyKey& key)  {

  yTable = egcache->scalarMultTable(key.y);

  if (!yTable) {
    // table cache is disabled; abort
    return;
  }

  kY = yTable->mult(key.k);
  kInv = key.k.invert();
}

size_t EGCacheImp::RekeyKey::hash::operator()(const pep::EGCacheImp::RekeyKey& k) const {
  size_t ret = 0;
  boost::hash_combine(ret, std::hash<CurvePoint>{}(k.y));
  boost::hash_combine(ret, std::hash<CurveScalar>{}(k.k));
  return ret;
}

EGCacheImp::TableValue::TableValue(EGCacheImp* egcache, const CurvePoint& p) {
  table_ = std::make_shared<CurvePoint::ScalarMultTable>(p);
}

EGCache::Metrics EGCacheImp::getMetrics() {
  EGCache::Metrics result;

  rskCache_.fillMetrics(result.rsk);
  tableCache_.fillMetrics(result.table);

  return result;
}


}
