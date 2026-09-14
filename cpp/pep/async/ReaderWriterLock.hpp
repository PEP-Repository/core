#pragma once

#include <boost/noncopyable.hpp>
#include <shared_mutex>

namespace pep {

/// @brief Reader/writer lock
/// @remark After https://stackoverflow.com/a/6450579.
/// @warning This class will (likely) starve the writer(s) under high read loads: see https://stackoverflow.com/questions/244316/reader-writer-locks-in-c#comment114678482_6450579.
class ReaderWriterLock : boost::noncopyable {
private:
  std::shared_mutex mutex_;

  using WriterLock = std::unique_lock<decltype(mutex_)>;
  using ReaderLock = std::shared_lock<decltype(mutex_)>;

  template <typename TLock, typename TFunction, typename... TArgs>
  std::invoke_result_t<TFunction, TArgs...> lockAndInvoke(TFunction function, TArgs... args) {
    TLock lock(mutex_);
    return function(std::forward<TArgs>(args)...);
  }

public:
  template <typename TFunction, typename... TArgs>
  std::invoke_result_t<TFunction, TArgs...> read(TFunction function, TArgs... args) {
    return lockAndInvoke<ReaderLock>(std::forward<TFunction>(function), std::forward<TArgs>(args)...);
  }

  template <typename TFunction, typename... TArgs>
  std::invoke_result_t<TFunction, TArgs...> write(TFunction function, TArgs... args) {
    return lockAndInvoke<WriterLock>(std::forward<TFunction>(function), std::forward<TArgs>(args)...);
  }
};

}
