#pragma once

#include <pep/utils/Platform.hpp>

#include <optional>
#include <string>
#include <filesystem>
#include <utility>

namespace pep {

#ifdef _WIN32

namespace win32api {

enum ElevationState {
  CANNOT_ELEVATE = 0,
  CAN_ELEVATE,
  IS_ELEVATED
};

std::string FormatWin32Error(DWORD code);

class ApiCallFailure : public std::runtime_error {
private:
  DWORD mCode;

private:
  ApiCallFailure(DWORD code, const std::string& description);

public:
  DWORD getCode() const noexcept { return mCode; }

  [[noreturn]] static void Raise(DWORD code);
  [[noreturn]] static void RaiseLastError();
  [[noreturn]] static void RaiseUnexpectedSuccess(const std::string& functionName);
};

std::wstring Utf8StringToWide(std::string_view utf8);
std::string WideStringToUtf8(std::wstring_view wide);

std::filesystem::path GetTempDirectory();
std::filesystem::path GetUniqueTemporaryPath();
std::filesystem::path GetUniqueTemporaryFileName();
std::filesystem::path CreateTemporaryDirectory();

enum KnownFolder {
  RoamingAppData,
  ProgramFiles
};

std::filesystem::path GetKnownFolderPath(KnownFolder folder);

void Download(const std::string& url, const std::filesystem::path& destination, bool allowCached = false);

ElevationState GetElevationState();
void StartProcess(const std::filesystem::path& start, const std::optional<std::string>& parameters = std::nullopt, bool elevate = false, bool callerProvidesMessageLoop = false);

// RAII wrapper to bind a (Windows subsystem) process's stdio to the parent console. See https://stackoverflow.com/a/55875595
class ParentConsoleBinding {
private:
  static ParentConsoleBinding* instance_;
  ParentConsoleBinding();
public:
  ~ParentConsoleBinding() noexcept;
  static std::unique_ptr<ParentConsoleBinding> TryCreate();
};

class SetConsoleCodePage {
  UINT prevInputCodePage{}, prevOutputCodePage{};

  /// Set console code pages
  /// \throws std::runtime_error when code page could not be set
  [[nodiscard]] SetConsoleCodePage(UINT newCodePage, UINT prevInputCodePage, UINT prevOutputCodePage);

public:
  SetConsoleCodePage() = default;
  /// Set console code pages
  /// \returns Object which reverts the code pages when destructed,
  /// unless no console was allocated
  /// \throws std::runtime_error when code page could not be set
  [[nodiscard]] SetConsoleCodePage(UINT codePage);

  // Delete copying ops
  SetConsoleCodePage(const SetConsoleCodePage&) = delete;
  SetConsoleCodePage& operator=(const SetConsoleCodePage&) = delete;

  SetConsoleCodePage(SetConsoleCodePage&& other) noexcept
    : prevInputCodePage{ std::exchange(other.prevInputCodePage, {}) },
    prevOutputCodePage{ std::exchange(other.prevOutputCodePage, {}) } {}
  SetConsoleCodePage& operator=(SetConsoleCodePage&&) = delete;

  /// Revert console code pages
  ~SetConsoleCodePage() noexcept;
};

void ClearMemory(void* address, size_t bytes);

template <typename T>
class SecureBuffer {
private:
  T* mAddress;
  size_t mMaxItems;

  SecureBuffer& operator=(const SecureBuffer& other) = delete;
  size_t getByteSize() const { return mMaxItems * sizeof(T); }

public:
  explicit SecureBuffer(size_t maxItems)
    : mAddress(nullptr), mMaxItems(maxItems) {
    if (mMaxItems != 0U) {
      mAddress = static_cast<T*>(malloc(getByteSize()));
      if (mAddress == nullptr) {
        throw std::bad_alloc();
      }
    }
  }

  SecureBuffer(const SecureBuffer& other)
    : SecureBuffer(other.maxItems()) {
    memcpy(mAddress, other.mAddress, getByteSize());
  }

  SecureBuffer(SecureBuffer&& other)
    : mAddress(other.mAddress), mMaxItems(other.mMaxItems) {
    other.mAddress = nullptr;
    other.mMaxItems = 0U;
  }

  ~SecureBuffer() noexcept {
    if (mAddress != nullptr) {
      ClearMemory(mAddress, getByteSize());
      free(mAddress);
      // No need to update mAddress: this instance is being destroyed anyway
    }
  }

  T* getAddress() noexcept { return mAddress; }
  const T* getAddress() const noexcept { return mAddress; }
  size_t getMaxItems() const noexcept { return mMaxItems; }
};

}

#endif // _WIN32

}
