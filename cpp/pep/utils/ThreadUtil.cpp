#include <pep/utils/ThreadUtil.hpp>
#ifdef _WIN32
# include <pep/utils/Win32Api.hpp>
# include <processthreadsapi.h>
#elif defined(__EMSCRIPTEN__)
# include <emscripten/threading.h>
# include <pthread.h>
#elif defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
# include <pthread.h>
#endif

namespace pep {

namespace {

thread_local std::optional<std::string> threadName;

}

const std::optional<std::string> &ThreadName::Get() {
  return threadName;
}

void ThreadName::Set(const std::string &name) {
#ifdef _WIN32
  (void)::SetThreadDescription(::GetCurrentThread(), win32api::Utf8StringToWide(name).c_str());
#elif defined(__APPLE__) && defined(__MACH__)
  ::pthread_setname_np(name.c_str());
#elif defined(__EMSCRIPTEN__)
  ::emscripten_set_thread_name(::pthread_self(), name.c_str());
#elif defined(__linux__)
  (void)::pthread_setname_np(::pthread_self(), name.c_str());
#endif
  threadName = name;
}

}
