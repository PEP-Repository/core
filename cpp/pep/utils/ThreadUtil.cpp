#include <pep/utils/ThreadUtil.hpp>
#ifdef _WIN32
#include <pep/utils/Win32Api.hpp>
#include <processthreadsapi.h>
#elif defined(__gnu_linux__) || (defined(__APPLE__) && defined(__MACH__))
#include <pthread.h>
#endif

namespace pep {
const std::optional<std::string> &ThreadName::Get() {
  return mName;
}

void ThreadName::Set(const std::string &name) {
#ifdef _WIN32
  (void)::SetThreadDescription(::GetCurrentThread(), win32api::Utf8StringToWide(name).c_str());
#elif defined(__APPLE__) && defined(__MACH__)
  ::pthread_setname_np(name.c_str());
#elif defined(__linux__)
  (void)::pthread_setname_np(::pthread_self(), name.c_str());
#endif
  mName = name;
}

thread_local std::optional<std::string> ThreadName::mName = std::nullopt;

}
