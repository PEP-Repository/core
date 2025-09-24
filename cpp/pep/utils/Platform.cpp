#include <pep/utils/Platform.hpp>

#ifdef _WIN32
tm* ::gmtime_r(const time_t* timer, tm* buf) noexcept {
  // https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/gmtime-s-gmtime32-s-gmtime64-s
  const auto err = gmtime_s(buf, timer);
  _set_errno(err);
  return err ? nullptr : buf;
}

tm* ::localtime_r(const time_t* timer, tm* buf) noexcept {
  // https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/localtime-s-localtime32-s-localtime64-s
  const auto err = localtime_s(buf, timer);
  _set_errno(err);
  return err ? nullptr : buf;
}

#endif
