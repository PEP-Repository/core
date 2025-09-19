#include <pep/utils/Defer.hpp>
#include <pep/utils/Random.hpp>
#include <pep/utils/Win32Api.hpp>
#include <pep/utils/Log.hpp>

#ifdef _WIN32
#include <iostream>

#include <Windows.h>
#include <wininet.h>
#pragma comment(lib, "wininet")
#include <urlmon.h>
#pragma comment(lib, "Urlmon")
#include <shellapi.h>
#include <Shlobj.h>
#endif

namespace pep {

#ifdef _WIN32

namespace win32api {

namespace {

const std::string LOG_TAG = "Win32Api";

GUID KnownFolderToFolderId(KnownFolder folder) {
  switch (folder) {
  case KnownFolder::RoamingAppData:
    return FOLDERID_RoamingAppData;
  case KnownFolder::ProgramFiles:
    return FOLDERID_ProgramFiles;
  default:
    throw std::runtime_error("Unsupported known folder value " + std::to_string(folder));
  }
}

bool RedirectConsoleIO()
{
  bool result = true;
  FILE* fp;

  // Redirect STDIN if the console has an input handle
  if (::GetStdHandle(STD_INPUT_HANDLE) != INVALID_HANDLE_VALUE) {
    if (freopen_s(&fp, "CONIN$", "r", stdin) != 0)
      result = false;
    else
      std::setvbuf(stdin, nullptr, _IONBF, 0);
  }

  // Redirect STDOUT if the console has an output handle
  if (::GetStdHandle(STD_OUTPUT_HANDLE) != INVALID_HANDLE_VALUE) {
    if (freopen_s(&fp, "CONOUT$", "w", stdout) != 0)
      result = false;
    else
      std::setvbuf(stdout, nullptr, _IONBF, 0);
  }

  // Redirect STDERR if the console has an error handle
  if (::GetStdHandle(STD_ERROR_HANDLE) != INVALID_HANDLE_VALUE) {
    if (freopen_s(&fp, "CONOUT$", "w", stderr) != 0)
      result = false;
    else
      std::setvbuf(stderr, nullptr, _IONBF, 0);
  }

  // Make C++ standard streams point to console as well.
  std::ios::sync_with_stdio(true);

  // Clear the error state for each of the C++ standard streams.
  std::wcout.clear();
  std::cout.clear();
  std::wcerr.clear();
  std::cerr.clear();
  std::wcin.clear();
  std::cin.clear();

  return result;
}

bool ReleaseConsole()
{
  bool result = true;
  FILE* fp;

  // Just to be safe, redirect standard IO to NUL before releasing.

  // Redirect STDIN to NUL
  if (freopen_s(&fp, "NUL:", "r", stdin) != 0)
    result = false;
  else
    std::setvbuf(stdin, nullptr, _IONBF, 0);

  // Redirect STDOUT to NUL
  if (freopen_s(&fp, "NUL:", "w", stdout) != 0)
    result = false;
  else
    std::setvbuf(stdout, nullptr, _IONBF, 0);

  // Redirect STDERR to NUL
  if (freopen_s(&fp, "NUL:", "w", stderr) != 0)
    result = false;
  else
    std::setvbuf(stderr, nullptr, _IONBF, 0);

  // Detach from console
  if (!::FreeConsole())
    result = false;

  return result;
}

void AdjustConsoleBuffer(int16_t minLength)
{
  // Set the screen buffer to be big enough to scroll some text
  CONSOLE_SCREEN_BUFFER_INFO conInfo;
  ::GetConsoleScreenBufferInfo(::GetStdHandle(STD_OUTPUT_HANDLE), &conInfo);
  if (conInfo.dwSize.Y < minLength)
    conInfo.dwSize.Y = minLength;
  ::SetConsoleScreenBufferSize(::GetStdHandle(STD_OUTPUT_HANDLE), conInfo.dwSize);
}

bool AttachParentConsole(int16_t minLength)
{
  bool result = false;

  // Release any current console and redirect IO to NUL
  ReleaseConsole();

  // Attempt to attach to parent process's console
  if (::AttachConsole(ATTACH_PARENT_PROCESS))
  {
    AdjustConsoleBuffer(minLength);
    result = RedirectConsoleIO();
  }

  return result;
}

}

std::string FormatWin32Error(DWORD code) {
  // Adapted from https://stackoverflow.com/a/17387176
  LPSTR messageBuffer = nullptr;
  size_t size = ::FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, nullptr);
  PEP_DEFER(::LocalFree(messageBuffer));

  return {messageBuffer, size};
}

ApiCallFailure::ApiCallFailure(DWORD code, const std::string& description)
  : std::runtime_error(description), mCode(code) {
}

void ApiCallFailure::Raise(DWORD code) {
  throw ApiCallFailure(code, FormatWin32Error(code));
}

void ApiCallFailure::RaiseLastError() {
  Raise(::GetLastError());
}

void ApiCallFailure::RaiseUnexpectedSuccess(const std::string& functionName) {
  throw std::runtime_error("API function '" + functionName + "' succeeded unexpectedly");
}

std::wstring Utf8StringToWide(std::string_view utf8) {
  if (utf8.empty()) {
    return {};
  }
  if (utf8.length() > size_t{std::numeric_limits<int>::max()}) {
    throw std::runtime_error("utf8 string too long to encode as wide string");
  }
  const int utf8Len = static_cast<int>(utf8.length());

  const int sizeNeeded = ::MultiByteToWideChar(
    CP_UTF8, MB_ERR_INVALID_CHARS,
    utf8.data(), utf8Len,
    {}, 0);
  if (!sizeNeeded) {
    ApiCallFailure::RaiseLastError();
  }
  assert(sizeNeeded > 0);
  std::wstring wide(static_cast<size_t>(sizeNeeded), '\0');
  const auto ret = ::MultiByteToWideChar(
    CP_UTF8, MB_ERR_INVALID_CHARS,
    utf8.data(), utf8Len,
    wide.data(), sizeNeeded);
  if (!ret) {
    ApiCallFailure::RaiseLastError();
  }
  return wide;
}

std::string WideStringToUtf8(std::wstring_view wide) {
  if (wide.empty()) {
    return {};
  }
  if (wide.length() > size_t{std::numeric_limits<int>::max()}) {
    throw std::runtime_error("wide string too long to encode as narrow string");
  }
  const int wideLen = static_cast<int>(wide.length());

  const int sizeNeeded = ::WideCharToMultiByte(
    CP_UTF8, WC_ERR_INVALID_CHARS,
    wide.data(), wideLen,
    {}, 0, nullptr, nullptr);
  if (!sizeNeeded) {
    ApiCallFailure::RaiseLastError();
  }
  assert(sizeNeeded > 0);
  std::string utf8(static_cast<size_t>(sizeNeeded), '\0');
  const auto ret = ::WideCharToMultiByte(
    CP_UTF8, WC_ERR_INVALID_CHARS,
    wide.data(), wideLen,
    utf8.data(), sizeNeeded, nullptr, nullptr);
  if (!ret) {
    ApiCallFailure::RaiseLastError();
  }
  return utf8;
}

std::filesystem::path GetTempDirectory() {
  char path[MAX_PATH];

  std::filesystem::path result;

  auto length = ::GetTempPathA(MAX_PATH, path);
  if (length == 0U) {
    win32api::ApiCallFailure::RaiseLastError();
  }
  if (length > MAX_PATH) {
    throw std::runtime_error("Temp path too long to fit buffer");
  }

  return std::filesystem::path(path);
}

std::filesystem::path GetUniqueTemporaryPath() {
  auto directory = GetTempDirectory().string();
  char path[MAX_PATH];

  do {
    UINT uUnique;
    pep::RandomBytes(reinterpret_cast<uint8_t *>(&uUnique), sizeof(uUnique));

    if (::GetTempFileNameA(directory.c_str(), "PTF" /* PEP temporary file */, uUnique, path) == 0U) {
      win32api::ApiCallFailure::RaiseLastError();
    }
  } while (std::filesystem::exists(path));

  return std::filesystem::path(path);
}

std::filesystem::path GetUniqueTemporaryFileName() {
  return GetUniqueTemporaryPath();
}

std::filesystem::path CreateTemporaryDirectory() {
  auto path = GetUniqueTemporaryPath();
  std::filesystem::create_directories(path);
  return path.string() + "\\";
}

std::filesystem::path GetKnownFolderPath(KnownFolder folder) {
  PWSTR ppszPath = nullptr;
  auto folderid = KnownFolderToFolderId(folder);
  if (::SHGetKnownFolderPath(folderid, 0, nullptr, &ppszPath) != S_OK) {
    throw std::runtime_error("Could not determine AppData path");
  }
  PEP_DEFER(::CoTaskMemFree(ppszPath));

  return std::filesystem::path(WideStringToUtf8(ppszPath));
}

void Download(const std::string& url, const std::filesystem::path& destination, bool allowCached) {
  if (!allowCached) {
    if (!::DeleteUrlCacheEntry(url.c_str())) {
      auto error = ::GetLastError();
      if (error != ERROR_FILE_NOT_FOUND) {
        ApiCallFailure::Raise(error);
      }
    }
  }
  auto file = destination.string();
  if (::URLDownloadToFileA(nullptr, url.c_str(), file.c_str(), 0, nullptr) != S_OK) {
    throw std::runtime_error("Failed to download " + url);
  }
}

ElevationState GetElevationState() {
  // Adapted from https://stackoverflow.com/a/50564639. Comments in switch cases were pasted from there
  HANDLE hToken;
  if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &hToken) == 0) {
    win32api::ApiCallFailure::RaiseLastError();
  }
  PEP_DEFER(::CloseHandle(hToken));

  TOKEN_ELEVATION_TYPE tet;
  DWORD returnLength;
  if (::GetTokenInformation(hToken, TokenElevationType, &tet, sizeof(tet), &returnLength) == 0) {
    win32api::ApiCallFailure::RaiseLastError();
  }
  if (returnLength != sizeof(tet)) {
    throw std::runtime_error("Token elevation type not retrieved correctly");
  }

  ElevationState result;

  switch (tet) {
  case TokenElevationTypeDefault:
    TOKEN_ELEVATION te;
    if (::GetTokenInformation(hToken, TokenElevation, &te, sizeof(te), &returnLength) == 0) {
      win32api::ApiCallFailure::RaiseLastError();
    }
    if (returnLength != sizeof(te)) {
      throw std::runtime_error("Token elevation (state) not retrieved correctly");
    }
    if (te.TokenIsElevated) {
      // we are built-in admin or UAC is disabled in system
      result = ElevationState::IS_ELEVATED;
    }
    else {
      // we can not elevate under same user
      result = ElevationState::CANNOT_ELEVATE;
    }
    break;

  case TokenElevationTypeFull:
    result = ElevationState::IS_ELEVATED;
    break;

  case TokenElevationTypeLimited:
    // this mean that we have linked token
    result = ElevationState::CAN_ELEVATE;
    break;

  default:
    throw std::runtime_error("Unsupported elevation type reported by OS");
  }

  return result;
}

void StartProcess(const std::filesystem::path& start, const std::optional<std::string>& parameters, bool elevate, bool callerProvidesMessageLoop) {
  auto cmd = start.string();

  SHELLEXECUTEINFO shExInfo = { 0 };
  shExInfo.cbSize = sizeof(shExInfo);
  shExInfo.fMask = SEE_MASK_DEFAULT;
  shExInfo.hwnd = 0;
  shExInfo.lpVerb = nullptr;
  shExInfo.lpFile = cmd.c_str();
  shExInfo.lpParameters = nullptr;
  shExInfo.lpDirectory = 0;
  shExInfo.nShow = SW_SHOW;
  shExInfo.hInstApp = 0;

  if (parameters) {
    shExInfo.lpParameters = parameters->c_str();
  }

  if (elevate) {
    switch (GetElevationState()) {
    case CANNOT_ELEVATE:
      throw std::runtime_error("Cannot start elevated process because current process is running under a limited account");
    case CAN_ELEVATE:
      shExInfo.lpVerb = "runas"; // https://stackoverflow.com/a/4893508
      break;
    case IS_ELEVATED:
      // Child process will run in an elevated context when using default verb ("open")
      break;
    default:
      throw std::runtime_error("Process running under unsupported elevation state");
    }
  }

  if (!callerProvidesMessageLoop) {
    shExInfo.fMask = SEE_MASK_NOASYNC; // "must be specified if [caller] does not have a message loop or if [caller] will terminate soon": see https://docs.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-shellexecuteinfoa
  }

  if (!::ShellExecuteEx(&shExInfo)) {
    // TODO: deal with "The operation was canceled by the user."
    win32api::ApiCallFailure::RaiseLastError();
  }
}

void ClearMemory(void* address, size_t bytes) {
  if (address != nullptr) {
    ::SecureZeroMemory(address, bytes);
  }
  else if (bytes != 0U) {
    throw std::runtime_error("nullptr cannot refer to a nonempty memory block");
  }
}

ParentConsoleBinding* ParentConsoleBinding::instance_ = nullptr;

ParentConsoleBinding::ParentConsoleBinding() {
  assert(instance_ == nullptr);
  instance_ = this;
}

ParentConsoleBinding::~ParentConsoleBinding() noexcept {
  assert(instance_ == this);
  instance_ = nullptr;
  ReleaseConsole();
}

std::unique_ptr<ParentConsoleBinding> ParentConsoleBinding::TryCreate() {
  if (instance_ != nullptr) {
    return nullptr;
  }
  if (!AttachParentConsole(1024)) {
    return nullptr;
  }

  return std::unique_ptr<ParentConsoleBinding>(new ParentConsoleBinding());
}

SetConsoleCodePage::SetConsoleCodePage(UINT codePage) {
  prevInputCodePage = ::GetConsoleCP();
  prevOutputCodePage = ::GetConsoleOutputCP();
  if (prevInputCodePage == 0 || prevOutputCodePage == 0) {
    return; // Probably no console allocated
  }
  if (!::SetConsoleCP(codePage)) {
    ApiCallFailure::RaiseLastError();
  } else if (!::SetConsoleOutputCP(codePage)) {
    const auto err = ::GetLastError();
    // Restore
    if (!::SetConsoleCP(prevInputCodePage)) {
      LOG(LOG_TAG, warning) << "Failed to restore console input code page (" << FormatWin32Error(::GetLastError()) << ") handing error";
    }
    ApiCallFailure::Raise(err);
  }
}

SetConsoleCodePage::~SetConsoleCodePage() noexcept {
  if (prevInputCodePage != 0 && !::SetConsoleCP(prevInputCodePage)) {
    LOG(LOG_TAG, warning) << "Failed to restore console input code page (" << FormatWin32Error(::GetLastError()) << ")";
  }
  if (prevOutputCodePage != 0 && !::SetConsoleOutputCP(prevOutputCodePage)) {
    LOG(LOG_TAG, warning) << "Failed to restore console output code page (" << FormatWin32Error(::GetLastError()) << ")";
  }
}

}

#endif

}
