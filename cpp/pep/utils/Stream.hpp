#pragma once

#ifdef _WIN32
# include <cstdio>
# include <iostream>
#endif

namespace pep {

class [[nodiscard]] SetBinaryFileMode {
private:

#ifdef _WIN32

  FILE* file_{};
  std::streambuf* stream_{};
  int prevMode_{};

  SetBinaryFileMode(FILE* file, std::streambuf* stream);

#else // Entire class is a no-op: only Windows differentiates between binary and text modes

  SetBinaryFileMode() noexcept = default;

#endif

public:
  SetBinaryFileMode(const SetBinaryFileMode&) = delete;
  SetBinaryFileMode& operator=(const SetBinaryFileMode&) = delete;
  SetBinaryFileMode(SetBinaryFileMode&& other) noexcept;
  SetBinaryFileMode& operator=(SetBinaryFileMode&&) = delete;

  ~SetBinaryFileMode() noexcept;

  static SetBinaryFileMode ForStdin();
  static SetBinaryFileMode ForStdout();
};


class CroppingStreamBuf : public std::streambuf {
private:
  std::streambuf& source_;
  std::streamsize remaining_;

protected:
  std::streamsize xsgetn(char* s, std::streamsize count) override;

public:
  CroppingStreamBuf(std::streambuf& source, std::streamsize count);

  CroppingStreamBuf(const CroppingStreamBuf&) = delete;
  CroppingStreamBuf& operator=(const CroppingStreamBuf&) = delete;
};

} // namespace pep
