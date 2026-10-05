#include <pep/gui/InterProcess.hpp>
#include <pep/utils/Defer.hpp>

#include <stdexcept>
#include <QSystemSemaphore>

#define PEP_GET_RAW_INTERPROCESS_MEMORY_VALUE(destination) memcpy(destination, implementor_->constData(), size_)

void InterProcessMemory::lockedInvoke(std::function<void()> callback) const {
  if (!implementor_->lock()) {
    throw std::runtime_error("Could not lock shared memory");
  }
  PEP_DEFER(if (!implementor_->unlock()) throw std::runtime_error("Could not unlock shared memory"));
  callback();
}

void InterProcessMemory::read(void* data) const {
  lockedInvoke([this, data]() {PEP_GET_RAW_INTERPROCESS_MEMORY_VALUE(data); });
}

void InterProcessMemory::write(const void* data, void* oldData) {
  lockedInvoke([this, data, oldData] {
    if (oldData != nullptr) {
      PEP_GET_RAW_INTERPROCESS_MEMORY_VALUE(oldData);
    }
    memcpy(implementor_->data(), data, size_);
  });
}

InterProcessMemory::InterProcessMemory(const QString& id, const void* initData, size_t size, QObject* parent)
  : QObject(parent), implementor_(new QSharedMemory(id, this)), size_(size) {
  // If we create() the QSharedMemory (below), ensure we can initialize it before another process attach()es and reads the (uninitialized) contents
  QSystemSemaphore sem("PEP initialization semaphore for QSharedMemory " + id, 1, QSystemSemaphore::Open);
  if (!sem.acquire()) {
    throw std::runtime_error("Failed to acquire semaphore");
  }
  PEP_DEFER(if (!sem.release()) throw std::runtime_error("Failed to release semaphore"));

  createOrAttach(initData, size, true);
}

void InterProcessMemory::createOrAttach(const void* initData, size_t size, bool initialAttempt) {
  creator_ = implementor_->create(static_cast<int>(size)); // TODO: verify that cast is possible
  if (creator_) {
    write(initData); // Semaphore ensures that this is done before another process tries to read the memory we just created
  }
  else if (implementor_->error() != QSharedMemory::AlreadyExists) {
    throw std::runtime_error("Failed to create shared memory");
  }
  else if (!implementor_->attach(QSharedMemory::AccessMode::ReadWrite)) {
    throw std::runtime_error("Failed to attach to shared memory");
  }
#ifndef _WIN32
  // We may have attached to memory that wasn't cleaned up properly after its previous owner(s) died.
  // See the "platform differences" described on https://doc.qt.io/qt-6/qsharedmemory.html#details .
  // In this case we'll discard the lingering value, create and initialize a new instance, and take
  // ownership/creatorship. See https://stackoverflow.com/a/42551052 .
  else if (initialAttempt) {
    if (!implementor_->detach()) { // This should delete the shm if no process use it
      throw std::runtime_error("Failed to detach from (possibly lingering) shared memory");
    }
    createOrAttach(initData, size, false);
  }
#endif
}
