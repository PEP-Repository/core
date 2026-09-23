#pragma once

#include <pep/utils/Action.hpp>
#include <pep/utils/ScopedResource.hpp>
#include <boost/noncopyable.hpp>
#include <map>
#include <mutex>
#include <shared_mutex>

namespace pep {

/// \brief Reader/writer lock
/// \remark After https://stackoverflow.com/a/6450579.
/// \warning This class will (likely) starve the writer(s) under high read loads: see https://stackoverflow.com/questions/244316/reader-writer-locks-in-c#comment114678482_6450579.
/// \warning This class will deadlock when used recursively and (at least) one of the locks is a writer lock.
class ReaderWriterLock : boost::noncopyable {
private:
  std::shared_mutex mutex_;

  using WriterLock = std::unique_lock<decltype(mutex_)>;
  using ReaderLock = std::shared_lock<decltype(mutex_)>;

  template <typename TLock>
  void lockAndPerform(Action action) {
    TLock lock(mutex_);
    action();
  }

public:
  template <typename TFunction, typename... TArgs>
  decltype(auto) read(TFunction function, TArgs... args) {
    return PerformAsAction(std::bind_front(&ReaderWriterLock::lockAndPerform<ReaderLock>, this), std::forward<TFunction>(function), std::forward<TArgs>(args)...);
  }

  template <typename TFunction, typename... TArgs>
  decltype(auto) write(TFunction function, TArgs... args) {
    return PerformAsAction(std::bind_front(&ReaderWriterLock::lockAndPerform<WriterLock>, this), std::forward<TFunction>(function), std::forward<TArgs>(args)...);
  }
};


/// \brief Reader/writer lock for synchronization between threads.
/// \remark This class supports recursive locking (with the exception of acquiring a writer lock when a reader lock is already held, in which case it will throw an exception).
class ThreadReaderWriterLock : boost::noncopyable {
private:
  ReaderWriterLock nonRecursive_;

  using Accessors = std::map<std::thread::id, unsigned>;
  Accessors readers_;
  Accessors writers_;
  std::mutex mutex_;

  /// \brief Ensures that the instance is locked by the calling thread for the specified type of access.
  /// \param write Whether caller needs write access (true) or only read access (false)
  /// \return A ScopedResource that must be kept alive for as long as caller needs the lock
  /// \remark Dereference the return value to determine if the lock was (first) acquired in this call
  ScopedResource<bool> acquire(bool write);

public:
  template <typename TFunction, typename... TArgs>
  decltype(auto) read(TFunction function, TArgs... args) {
    auto locked = this->acquire(false);
    if (*locked) {
      return nonRecursive_.read(std::forward<TFunction>(function), std::forward<TArgs>(args)...);
    }
    // else
    return function(std::forward<TArgs>(args)...);
  }

  template <typename TFunction, typename... TArgs>
  decltype(auto) write(TFunction function, TArgs... args) {
    auto locked = this->acquire(true);
    if (*locked) {
      return nonRecursive_.write(std::forward<TFunction>(function), std::forward<TArgs>(args)...);
    }
    // else
    return function(std::forward<TArgs>(args)...);
  }

};

}
