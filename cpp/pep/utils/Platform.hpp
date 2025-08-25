#pragma once

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
# define NOMINMAX
#endif
#include <Winsock2.h>
#include <Windows.h>
#include <Ws2ipdef.h>
#include <ctime>
#include <io.h>

// https://www.man7.org/linux/man-pages/man3/gmtime_r.3p.html
tm* gmtime_r(const time_t*, tm* buf) noexcept;
// https://www.man7.org/linux/man-pages/man3/localtime_r.3p.html
tm* localtime_r(const time_t*, tm* buf) noexcept;

// https://www.man7.org/linux/man-pages/man3/timegm.3.html
inline time_t timegm(tm *tm) noexcept {
  // https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/mkgmtime-mkgmtime32-mkgmtime64
  return _mkgmtime(tm);
}

#else
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <fcntl.h>
#include <cstdlib>
#include <libgen.h>
#if defined(__APPLE__) && defined(__MACH__)
#define OSATOMIC_DEPRECATED
#endif

#endif
