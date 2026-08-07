#pragma once

#include <pep/utils/Platform.hpp>

#include <optional>
#include <string>
#include <filesystem>
#include <utility>

namespace pep {

#ifdef _WIN32

namespace win32api {

enum class ElevationState {
  CannotElevate = 0,
  CanElevate,
  IsElevated
};

std::string FormatWin32Error(DWORD code);

class ApiCallFailure : public std::runtime_error {
private:
  DWORD code_;

private:
  ApiCallFailure(DWORD code, const std::string& description);

public:
  DWORD getCode() const noexcept { return code_; }

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
  ParentConsoleBinding();
public:
  ~ParentConsoleBinding() noexcept;
  static std::unique_ptr<ParentConsoleBinding> TryCreate();
};

class SetConsoleCodePage {
  UINT prevInputCodePage_{}, prevOutputCodePage_{};

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
    : prevInputCodePage_{ std::exchange(other.prevInputCodePage_, {}) },
    prevOutputCodePage_{ std::exchange(other.prevOutputCodePage_, {}) } {}
  SetConsoleCodePage& operator=(SetConsoleCodePage&&) = delete;

  /// Revert console code pages
  ~SetConsoleCodePage() noexcept;
};

void ClearMemory(void* address, size_t bytes);

template <typename T>
class SecureBuffer {
private:
  T* address_;
  size_t maxItems_;

  SecureBuffer& operator=(const SecureBuffer& other) = delete;
  size_t getByteSize() const { return maxItems_ * sizeof(T); }

public:
  explicit SecureBuffer(size_t maxItems)
    : address_(nullptr), maxItems_(maxItems) {
    if (maxItems_ != 0U) {
      address_ = static_cast<T*>(malloc(getByteSize()));
      if (address_ == nullptr) {
        throw std::bad_alloc();
      }
    }
  }

  SecureBuffer(const SecureBuffer& other)
    : SecureBuffer(other.maxItems()) {
    memcpy(address_, other.address_, getByteSize());
  }

  SecureBuffer(SecureBuffer&& other)
    : address_(other.address_), maxItems_(other.maxItems_) {
    other.address_ = nullptr;
    other.maxItems_ = 0U;
  }

  ~SecureBuffer() noexcept {
    if (address_ != nullptr) {
      ClearMemory(address_, getByteSize());
      free(address_);
      // No need to update address_: this instance is being destroyed anyway
    }
  }

  T* getAddress() noexcept { return address_; }
  const T* getAddress() const noexcept { return address_; }
  size_t getMaxItems() const noexcept { return maxItems_; }
};

}

#endif // _WIN32

}
