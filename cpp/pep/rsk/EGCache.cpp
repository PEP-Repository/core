#include <pep/rsk/EGCache.hpp>

#include <pep/crypto/Timestamp.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/Singleton.hpp>

#include <boost/core/noncopyable.hpp>
#include <boost/functional/hash.hpp>

#include <unordered_map>
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

static const std::string LOG_TAG("EGCache");


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
    // The EGCache is passed along, because RSKValue needs to access
    // the table cache.
    static_assert(std::is_constructible_v<Value, EGCacheImp*, const Key&>);

    // 'Value' should have an explicit operator bool that indicates whether
    // it was constructed successfully.
    // TODO: static_assert this.

    struct Entry {
      Entry(EGCacheImp* egcache, const Key& key, uint64_t generation)
        : value(egcache, key), lastUse(generation) {
      }

    private:
      Value value;
      std::atomic_uint64_t lastUse;

    public:
      inline uint64_t getLastUse() const {
        return this->lastUse.load(std::memory_order_relaxed);
      }

      inline void updateLastUse(uint64_t generation) {
        this->lastUse.store(generation, std::memory_order_relaxed);
      }

      inline Value& getValue() {
        return this->value;
      }

      inline explicit operator bool() const {
        return value ? true : false;
      }
    };

    std::shared_mutex mux;
    std::unordered_map<Key, Entry, KeyHash> data;
    uint64_t generation = 0;
    std::atomic_uint64_t useCount = 0; // for metrics
    steady_seconds
        lastPrune = TimeNow<steady_seconds>(),
        disabledAt = TimeNow<steady_seconds>();
    bool enabled = true;

    using iterator = typename decltype(data)::iterator;

    inline void incrementUseCount() {
      this->useCount.fetch_add(1, std::memory_order_relaxed);
    }

    inline uint64_t getUseCount() {
      return this->useCount.load(std::memory_order_relaxed);
    }

    inline void disable_under_unique_lock() {
      assert(this->enabled);
      this->enabled = false;
      this->disabledAt = TimeNow<steady_seconds>();
    }

    // if the cache is disabled, but the prune has cooled down,
    // the cache is said to be enablable.
    inline bool enabledOrEnablable_underSharedLock() {
      if (this->enabled)
        return true;
      return TimeNow<steady_seconds>() - this->disabledAt >= Options::ReEnableTime;
    }

    inline bool pruneCooledDown_underSharedLock() {
      return TimeNow<steady_seconds>() - this->lastPrune >= Options::PruneCooldown;
    }

    // prune cache, which might disable it
    void prune_under_unique_lock() {
      assert(this->enabled);
      if (!this->pruneCooledDown_underSharedLock()) {
        LOG(LOG_TAG, warning)
          << Options::Name
          << " cache overflows a second time in a short while. "
          << "Disabling " << Options::Name
          << " cache to mitigate potential DoS.";
          this->disable_under_unique_lock();
          return;
      }

      size_t toEvict = this->data.size() - Options::PrunedSize;
      std::vector<std::pair<uint64_t, Key>> entries;
      entries.reserve(this->data.size());
      for (const auto& pair : this->data)
        entries.emplace_back(pair.second.getLastUse(), pair.first);
      std::sort(entries.begin(), entries.end(),
          [](auto& a, auto& b) { return a.first < b.first; });
      for (size_t i = 0; i < toEvict; i++) {
        this->data.erase(entries[i].second);
      }

      LOG(LOG_TAG, info) << "Pruned " << Options::Name << " cache down to "
        << this->data.size();
      this->lastPrune = TimeNow<steady_seconds>();
    }

    // adds entry associated with the given key, and returns
    // an iterator to it.
    std::optional<iterator> cache_under_unique_lock(
        EGCacheImp* egcache, const Key& key) {

      assert(this->data.find(key) == this->data.end());
      assert(this->enabled);

      if (this->data.size() >= Options::MaxSize) {
        // since entries are added one by one:
        assert(this->data.size()==Options::MaxSize);
        this->prune_under_unique_lock();

        if (!this->enabled)
          return std::nullopt;
      }


      auto [it, inserted] = this->data.emplace(
          std::piecewise_construct,
          std::make_tuple(key),
          std::make_tuple(egcache, key, ++this->generation));

      assert(inserted);

      if (!it->second) {
        // Creation of the entry failed.  The creation of an RSK entry might
        // fail, for example, when the table cache is disabled.
        LOG(LOG_TAG, warning)
          << "Failed to add entry to " << Options::Name << " cache; "
            << "disabling. ";
        this->data.erase(it);
        this->disable_under_unique_lock();
        return std::nullopt;
      }

      LOG(LOG_TAG, debug)
        << "Entry added to " << Options::Name << " cache.  size: "
        << this->data.size()
          << "; generation: " << this->generation;

      return it;
    }

    // get cache entry associated with key, if it exists
    std::optional<iterator> get_under_shared_lock(const Key& key) {
      auto it = this->data.find(key);
      if (it != this->data.end()) {
        it->second.updateLastUse(this->generation);
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
        auto readLock = std::shared_lock(this->mux);

        auto it = this->get_under_shared_lock(key);

        if (it)
          return it;

        if (!this->enabledOrEnablable_underSharedLock())
          return std::nullopt;

        // key is not present, so we need to obtain a write lock to add it
      }

      auto writeLock = std::unique_lock(this->mux);

      // the key might have been added in the meantime
      auto it = this->get_under_shared_lock(key);

      if (it)
        return it;

      // the cache might have been disabled in the meantime.
      if (!this->enabledOrEnablable_underSharedLock())
        return std::nullopt;

      // enable cache, if necessary
      if (!this->enabled) {
        LOG(LOG_TAG, warning)
          << "Re-enabling " << Options::Name
          << " cache.";
        this->enabled = true;
      }

      return this->cache_under_unique_lock(egcache, key);
    }

    void fillMetrics(EGCache::Metrics::OfCache& metrics) {
      // Acquire unique lock to force all relaxed atomic
      // operations to materialize.
      // (A shared_lock would have suffices as well, if we do not care
      //  about accuracy.)
      auto uniqueLock = std::unique_lock(this->mux);

      metrics.generation = this->generation;
      metrics.useCount = this->getUseCount();
    }
  };


  // RSK Cache
  //
  // Caches 1/k, k*y and (reference to) scalar multiplication table of y.
  // Used to speed up the ElgamalEncryption::RSK operation and
  // ElgamalEncryption::rekey operations.
  struct RSKKey {
    RSKKey(CurveScalar k, CurvePoint y)
      : mK(k), mY(y) { }
    bool operator== (const RSKKey& p) const;

    CurveScalar mK;
    CurvePoint mY;

    struct hash { size_t operator()(const pep::EGCacheImp::RSKKey& k) const; };
  };

  struct RSKValue {
    RSKValue(EGCacheImp* egcache, const RSKKey& key);

    ElgamalEncryption RSK(const CurvePoint& b, const CurvePoint& c,
            const CurveScalar& z, CPRNG* rng) const;
    ElgamalEncryption RK(const CurvePoint& b, const CurvePoint& c,
            CPRNG* rng) const;

    CurveScalar mKInv;
    CurvePoint mKY;
    std::shared_ptr<CurvePoint::ScalarMultTable> mYTable;

    // returns whether the value was successfully initialized
    inline explicit operator bool() const {
      return this->mYTable ? true : false;
    }
  };

  struct RSKOptions {
    static constexpr size_t PrunedSize = 250;
    static constexpr size_t MaxSize = 500;
    static constexpr const char *Name = "RSK";
    static constexpr auto PruneCooldown = 15min;
    static constexpr auto ReEnableTime = 1h;
  };

  Cache<RSKKey, RSKValue, RSKOptions, RSKKey::hash> rskCache;


  // Scalar multiplication (table) cache
  struct TableValue {
    TableValue(EGCacheImp* egcache, const CurvePoint& p);
    std::shared_ptr<CurvePoint::ScalarMultTable> mTable;

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

  Cache<CurvePoint, TableValue, TableOptions> tableCache;

 public:
  ElgamalEncryption RSK(
    ElgamalEncryption eg,
    const CurveScalar& z,
    ElgamalTranslationKey k,
    CPRNG* rng=nullptr
  ) override;

  ElgamalEncryption RK(
    ElgamalEncryption eg,
    ElgamalTranslationKey k,
    CPRNG* rng=nullptr
  ) override;

  ElgamalEncryption rerandomize(
    ElgamalEncryption eg,
    CPRNG* rng=nullptr
  ) override;

  std::shared_ptr<CurvePoint::ScalarMultTable>
  scalarMultTable(const CurvePoint& b) override;

  EGCache::Metrics getMetrics() override;

  using StaticSingleton<EGCacheImp>::Instance;
};

EGCache& EGCache::get() {
  return EGCacheImp::Instance();
}


std::shared_ptr<CurvePoint::ScalarMultTable>
EGCacheImp::scalarMultTable(const CurvePoint& b) {

  auto optional_it = this->tableCache.get(this, CurvePoint(b));

  if (!optional_it)
    return nullptr;

  return (*optional_it)->second.getValue().mTable;
}

ElgamalEncryption EGCacheImp::RSK(
    ElgamalEncryption eg,
    const CurveScalar& z,
    ElgamalTranslationKey k,
    CPRNG* rng) {

  auto key = RSKKey(k, eg.y);

  auto opt_it = this->rskCache.get(this, key);

  if (!opt_it) {
    // fall back to uncached RSK
    return ElgamalEncryption(eg.b, eg.c, key.mY).RSK(z, key.mK);
  }

  return (*opt_it)->second.getValue().RSK(eg.b, eg.c, z, rng);
}

ElgamalEncryption EGCacheImp::RK(
    ElgamalEncryption eg,
    ElgamalTranslationKey k,
    CPRNG* rng) {

  auto key = RSKKey(k, eg.y);

  auto opt_it = this->rskCache.get(this, key);

  if (!opt_it) {
    // fall back to uncached RK
    return ElgamalEncryption(eg.b, eg.c, key.mY).rerandomize().rekey(key.mK);
  }

  return (*opt_it)->second.getValue().RK(eg.b, eg.c, rng);
}

ElgamalEncryption EGCacheImp::rerandomize(
    ElgamalEncryption eg,
    CPRNG* rng) {

  auto table = scalarMultTable(eg.y);
  ElgamalEncryption ret;

  if (table == nullptr)
    return eg.rerandomize();

  auto r = rng == nullptr ? CurveScalar::Random() : CurveScalar::Random<>(*rng);
  ret.b = eg.b.add(CurvePoint::BaseMult(r));
  ret.c = eg.c.add(table->mult(r));
  ret.y = eg.y;
  return ret;
}

bool EGCacheImp::RSKKey::operator== (const EGCacheImp::RSKKey& k) const {
  return k.mK == mK && k.mY == mY;
}

ElgamalEncryption EGCacheImp::RSKValue::RK(
    const CurvePoint& b,
    const CurvePoint& c,
    CPRNG* rng) const {
  ElgamalEncryption ret;

  // ret.b = 1/k (b + r B)
  auto r = rng == nullptr ? CurveScalar::Random() : CurveScalar::Random<>(*rng);
  auto bPlusRB = CurvePoint::BaseMult(r).add(b);
  ret.b = bPlusRB.mult(mKInv);

  // ret.c = c + ry
  auto ry = mYTable->mult(r);
  ret.c = c.add(ry);

  // ret.y = ky
  ret.y = mKY;
  return ret;
}

ElgamalEncryption EGCacheImp::RSKValue::RSK(
    const CurvePoint& b,
    const CurvePoint& c,
    const CurveScalar& z,
    CPRNG* rng) const {
  ElgamalEncryption ret;

  // ret.b = (z * 1/k) (b + r B)
  auto r = rng == nullptr ? CurveScalar::Random() : CurveScalar::Random<>(*rng);
  auto zOverK = mKInv.mult(z);
  auto bPlusRB = CurvePoint::BaseMult(r).add(b);
  ret.b = bPlusRB.mult(zOverK);

  // ret.c = z(c + ry)
  auto ry = mYTable->mult(r);
  ret.c = c.add(ry).mult(z);

  // ret.y = ky
  ret.y = mKY;
  return ret;
}

EGCacheImp::RSKValue::RSKValue(
    EGCacheImp* egcache, const EGCacheImp::RSKKey& key)  {

  this->mYTable = egcache->scalarMultTable(key.mY);

  if (!this->mYTable) {
    // table cache is disabled; abort
    return;
  }

  this->mKY = this->mYTable->mult(key.mK);
  this->mKInv = key.mK.invert();
}

size_t EGCacheImp::RSKKey::hash::operator()(const pep::EGCacheImp::RSKKey& k) const {
  size_t ret = 0;
  boost::hash_combine(ret, std::hash<CurvePoint>{}(k.mY));
  boost::hash_combine(ret, std::hash<CurveScalar>{}(k.mK));
  return ret;
}

EGCacheImp::TableValue::TableValue(EGCacheImp* egcache, const CurvePoint& p) {
  mTable = std::make_shared<CurvePoint::ScalarMultTable>(p);
}

EGCache::Metrics EGCacheImp::getMetrics() {
  EGCache::Metrics result;

  this->rskCache.fillMetrics(result.rsk);
  this->tableCache.fillMetrics(result.table);

  return result;
}


}
