#pragma once

#include <functional>
#include <QSharedMemory>

class InterProcessMemory : public QObject {
  Q_OBJECT

private:
  QSharedMemory* mImplementor = nullptr;
  size_t mSize = 0U;
  bool mCreator = false;

private:
  void lockedInvoke(std::function<void()> callback) const;
  void createOrAttach(const void* initData, size_t size, bool initialAttempt);

public:
  // Allocates a chunk of inter-process memory or attaches to an existing chunk. If the memory was allocated, its contents are initialized to initData.
  // Use the "isCreator" method to determine if the memory was allocated by this instance.
  InterProcessMemory(const QString& id, const void* initData, size_t size, QObject* parent = nullptr);

  // Prevent shallow copies (deep copies currently not supported)
  InterProcessMemory(const InterProcessMemory& other) = delete;
  InterProcessMemory& operator =(const InterProcessMemory& other) = delete;

  // Callers must ensure that arguments are sufficiently large
  void write(const void* data, void* oldData = nullptr);
  void read(void* data) const;

  inline bool isCreator() const noexcept { return mCreator; }
};


template <typename T>
class InterProcess : public QObject {
  static_assert(std::is_trivially_copyable<T>::value, "InterProcess<> template parameter must be trivially copyable");
  static_assert(std::is_default_constructible<T>::value, "InterProcess<> template parameter must be default constructible");

private:
  InterProcessMemory* mImplementor;

public:
  // Creates an inter-process value or attaches to an existing one. If the value was created, it is initialized to initValue.
  // Use the "createdValue" method to determine if the value was created by this instance.
  InterProcess(const QString& id, const T& initValue, QObject* parent = nullptr)
    : QObject(parent), mImplementor(new InterProcessMemory(id, &initValue, sizeof(const T), this)) {
  }

  // Prevent shallow copies (deep copies currently not supported)
  InterProcess(const InterProcess& other) = delete;
  InterProcess& operator =(const InterProcess& other) = delete;

  inline bool createdValue() const noexcept { return mImplementor->isCreator(); }

  T get() const {
    T result {};
    mImplementor->read(&result);
    return result;
  }

  T set(const T& value) { // Returns old value
    T result {};
    mImplementor->write(&value, &result);
    return result;
  }

  explicit operator T() const { return get(); }
  InterProcess& operator =(const T& value) { mImplementor->write(&value); return *this; }
};
