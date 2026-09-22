#pragma once

#include <pep/utils/Action.hpp>
#include <boost/noncopyable.hpp>
#include <mutex>
#include <shared_mutex>

namespace pep {

/// \brief Reader/writer lock
/// \remark After https://stackoverflow.com/a/6450579.
/// \warning This class will (likely) starve the writer(s) under high read loads: see https://stackoverflow.com/questions/244316/reader-writer-locks-in-c#comment114678482_6450579.
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

}
