#include <pep/async/ReaderWriterLock.hpp>

namespace pep {

ScopedResource<bool> ThreadReaderWriterLock::acquire(bool write) {
  bool first = true;
  auto self = std::this_thread::get_id();
  Accessors* accessors;

  { // Prevent other threads from updating our state while we're manipulating it
    std::scoped_lock lock(mutex_);

    if (write) {
      if (writers_[self] == 0U) { // If we're not holding a writer lock (already)...
        auto reading = readers_.find(self);
        if (reading != readers_.end() && reading->second != 0U) { // ... and we're reading...
          // ... then we're holding a reader lock, so we can't get a writer lock from our nonRecursive_ implementor
          throw std::runtime_error("Can't acquire writer lock because a reader lock is already held");
        }
      }
      accessors = &writers_;
    }
    else {
      auto pos = writers_.find(self);
      if (pos != writers_.end() && pos->second != 0U) {
        first = false; // We can read within the writer lock that we're already holding
      }
      accessors = &readers_;
    }

    // Increment access count
    if ((*accessors)[self]++ != 0U) {
      // Count for this type of access was nonzero: we're already holding a lock (of the required type) on our nonRecursive_ implementor
      first = false;
    }
  }

  // Return a ScopedResource that...
  return MakeScopedResource(
    [first]() { return first; }, // ... caller can dereference to determine if they need to acquire the (plain, nonrecursive) ReaderWriterLock...
    [this, self, accessors](bool) { // ... and that decreases the accessor count when caller discards it
      assert(self == std::this_thread::get_id());

      std::scoped_lock lock(mutex_);
      assert(accessors->find(self) != accessors->end());
      auto& count = (*accessors)[self];
      assert(count != 0U);
      --count;
    }
  );
}

}
