#include <pep/rsk/EGCache.hpp>

#include <pep/crypto/Timestamp.hpp>
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
    // The EGCache is passed along, because RekeyValue needs to access
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
        // Creation of the entry failed.  The creation of an RSK entry used to be able to
        // fail, for example, when the table cache was disabled.
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
  // Used to speed up the ElgamalEncryption::reshuffleRekey operation and
  // ElgamalEncryption::rekey operations.
  struct RekeyKey {
    RekeyKey(const CurveScalar& k, const CurvePoint& y)
      : mK(k), mY(y) { }
    bool operator== (const RekeyKey& p) const;

    CurveScalar mK;
    CurvePoint mY;

    struct hash { size_t operator()(const pep::EGCacheImp::RekeyKey& k) const; };
  };

  struct RekeyValue {
    RekeyValue(EGCacheImp* egcache, const RekeyKey& key);

    ElgamalEncryption reshuffleRekey(
            const CurvePoint& b, const CurvePoint& c,
            const ElgamalTranslationKey& reshuffle) const;
    ElgamalEncryption rekey(const CurvePoint& b, const CurvePoint& c) const;

    CurveScalar mKInv;
    CurvePoint mKY;

    // returns whether the value was successfully initialized.
    // Used to fail when table cache for public key was disabled,
    // which was used for rerandomization
    inline explicit operator bool() const {
      return true;
    }
  };

  struct RSKOptions {
    static constexpr size_t PrunedSize = 250;
    static constexpr size_t MaxSize = 500;
    static constexpr const char *Name = "RSK";
    static constexpr auto PruneCooldown = 15min;
    static constexpr auto ReEnableTime = 1h;
  };

  Cache<RekeyKey, RekeyValue, RSKOptions, RekeyKey::hash> rskCache;


  // Scalar multiplication (table) cache
  struct TableValue {
    TableValue(EGCacheImp* egcache, const CurvePoint& p);
    std::shared_ptr<CurvePoint::ScalarMultTable> mTable;

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
  ElgamalEncryption reshuffleRekey(
    const ElgamalEncryption& eg,
    const CurveScalar& reshuffle,
    const ElgamalTranslationKey& rekey
  ) override;

  ElgamalEncryption rekey(
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

EGCache& EGCache::get() {
  return EGCacheImp::Instance();
}


std::shared_ptr<CurvePoint::ScalarMultTable>
EGCacheImp::scalarMultTable(const CurvePoint& b) {

  auto optional_it = this->tableCache.get(this, b);

  if (!optional_it)
    return nullptr;

  return (*optional_it)->second.getValue().mTable;
}

ElgamalEncryption EGCacheImp::reshuffleRekey(
    const ElgamalEncryption& eg,
    const CurveScalar& reshuffle,
    const ElgamalTranslationKey& rekey) {

  auto key = RekeyKey(rekey, eg.publicKey);

  auto opt_it = this->rskCache.get(this, key);

  if (!opt_it) {
    // fall back to uncached reshuffleRekey
    return ElgamalEncryption(eg.b, eg.c, key.mY).reshuffleRekey(reshuffle, key.mK);
  }

  return (*opt_it)->second.getValue().reshuffleRekey(eg.b, eg.c, reshuffle);
}

ElgamalEncryption EGCacheImp::rekey(
    const ElgamalEncryption& eg,
    const ElgamalTranslationKey& rekey) {

  auto key = RekeyKey(rekey, eg.publicKey);

  auto opt_it = this->rskCache.get(this, key);

  if (!opt_it) {
    // fall back to uncached rekey
    return ElgamalEncryption(eg.b, eg.c, key.mY).rekey(key.mK);
  }

  return (*opt_it)->second.getValue().rekey(eg.b, eg.c);
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
  return k.mK == mK && k.mY == mY;
}

ElgamalEncryption EGCacheImp::RekeyValue::rekey(
    const CurvePoint& b,
    const CurvePoint& c) const {
  return {
    mKInv * b,
    c,
    mKY,
  };
}

ElgamalEncryption EGCacheImp::RekeyValue::reshuffleRekey(
    const CurvePoint& b,
    const CurvePoint& c,
    const ElgamalTranslationKey& reshuffle) const {
  return {
    reshuffle * mKInv * b,
    reshuffle * c,
    mKY,
  };
}

EGCacheImp::RekeyValue::RekeyValue(
    EGCacheImp* egcache, const EGCacheImp::RekeyKey& key)  {

  // Since we don't do rerandomization anymore in the (R)SK, the table is not as useful anymore.
  // We should probably benchmark if it's better to just remove it, or maybe leave it for the global public key only.
  auto yTable = egcache->scalarMultTable(key.mY);

  this->mKY = yTable ? yTable->mult(key.mK) : key.mK * key.mY;
  this->mKInv = key.mK.invert();
}

size_t EGCacheImp::RekeyKey::hash::operator()(const pep::EGCacheImp::RekeyKey& k) const {
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
