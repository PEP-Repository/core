#include <pep/async/ReaderWriterLock.hpp>
#include <pep/utils/Defer.hpp>

namespace pep {

std::function<void(const Action&)> ThreadReaderWriterLock::makeLockingPerformer(bool write) {
  return [this, write](const Action& action) { return this->performLocked(write, action); };
}

void ThreadReaderWriterLock::performLocked(bool write, const Action& action) {
  bool acquire = true;
  std::function<void(ReaderWriterLock&, const Action&)> acquireAndInvoke;
  auto self = std::this_thread::get_id();
  Accessors* accessors;

  { // Prevent other threads from updating our state while we're manipulating it
    std::scoped_lock lock(mutex_);

    if (write) {
      if (writers_[self] == 0U) { // If we're not holding a writer lock...
        auto reading = readers_.find(self);
        if (reading != readers_.end() && reading->second != 0U) { // ... but we're holding a reader lock...
          // ... then we can't acquire a writer lock from our nonRecursive_ implementor (because it would deadlock)
          throw std::runtime_error("Can't acquire writer lock because a reader lock is already held");
        }
      }
      accessors = &writers_;
      acquireAndInvoke = [](ReaderWriterLock& nonrecursive, const Action& action) { return nonrecursive.write(action); };
    }
    else {
      auto pos = writers_.find(self);
      if (pos != writers_.end() && pos->second != 0U) {
        acquire = false; // We can read within the writer lock that we're already holding
      }
      accessors = &readers_;
      acquireAndInvoke = [](ReaderWriterLock& nonrecursive, const Action& action) { return nonrecursive.read(action); };
    }

    // Increment access count
    if ((*accessors)[self]++ != 0U) {
      // We're already holding a lock (of the required type) on our nonRecursive_ implementor
      acquire = false;
    }
  }

  // Ensure that access count is decremented after the invocation.
  // Defined in a separate lambda because the PEP_DEFER macro doesn't combine well with multi-line code.
  auto decrement = [&]() {
      std::scoped_lock lock(mutex_);
      (*accessors)[self]--;
    };
  PEP_DEFER(decrement());

  if (acquire) {
    acquireAndInvoke(nonRecursive_, action);
  }
  else { // We're already holding a (non-recursive) lock matching or exceeding the required access level
    action();
  }
}

}
