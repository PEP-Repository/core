#include <pep/utils/Platform.hpp>
#include <pep/utils/Random.hpp>

#include <chrono>
#include <system_error>
#include <thread>

using namespace std::chrono_literals;

namespace pep {

#ifndef _WIN32
class URandom {
 private:
  int fd;

 public:
  URandom() : fd(-1) {
    fd = open("/dev/urandom", O_RDONLY);
    if (fd == -1) {
      throw std::system_error(errno, std::generic_category());
    }
  }

  ~URandom() noexcept {
    close(fd);
  }

  void read(uint8_t* p, uint64_t len) const {
    ssize_t i;

    while (len > 0) {
      i = (len < (1 << 10)) ? static_cast<ssize_t>(len) : (1 << 10);
      i = ::read(fd, p, static_cast<size_t>(i));
      if (i < 1) {
        std::this_thread::sleep_for(100ms);
        continue;
      }

      p += i;
      len -= static_cast<uint64_t>(i);
    }
  }
};
#endif

/*! \brief Generates random bytes.
 *
 *  Reads random bytes from /dev/urandom.
 *
 *  \param [out] p The buffer to place the bytes in.
 *  \param len The number of bytes to generate.
 */
void RandomBytes(uint8_t* p, uint64_t len) {
  // use Rtl rather than Crypt since we want to avoid loading all the CryptAPI stuff
#ifdef _WIN32
  static HMODULE hAdvapi = nullptr;
  static BOOLEAN (WINAPI *pRtlGenRandom)(
    PVOID RandomBuffer,
    ULONG RandomBufferLength
  ) = nullptr;
  if (hAdvapi == nullptr)
    hAdvapi = ::LoadLibrary("Advapi32.dll");
  if (pRtlGenRandom == nullptr)
    pRtlGenRandom = (BOOLEAN (WINAPI*)(PVOID, ULONG))::GetProcAddress(hAdvapi, "SystemFunction036");

  if (pRtlGenRandom == nullptr || !pRtlGenRandom(p, static_cast<ULONG>(len))) {
    throw RandomException();
  }
#else
  static URandom urandom;
  urandom.read(p, len);
#endif
}

void RandomBytes(std::vector<char>& s, size_t len) {
  s.resize(len);
  RandomBytes(reinterpret_cast<uint8_t*>(s.data()), len);
}

void RandomBytes(std::string& s, size_t len) {
  s.resize(len);
  RandomBytes(reinterpret_cast<uint8_t*>(s.data()), len);
}

std::string RandomString(size_t len)
{
  std::string result(len, '\0');
  RandomBytes(result, len);
  return result;
}

}
