#pragma once

#include <pep/utils/Hasher.hpp>
#include <streambuf>

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


class CroppingIStreamBuf : public std::streambuf {
private:
  std::streambuf& source_;
  std::streamsize remaining_;

protected:
  std::streamsize xsgetn(char* s, std::streamsize count) override;

public:
  CroppingIStreamBuf(std::streambuf& source, std::streamsize count);

  CroppingIStreamBuf(const CroppingIStreamBuf&) = delete;
  CroppingIStreamBuf& operator=(const CroppingIStreamBuf&) = delete;
};


class HashingIStreamBuf : public std::streambuf {
private:
  std::streambuf& source_;
  HasherBase& hasher_;

protected:
  std::streamsize xsgetn(char* s, std::streamsize count) override;

public:
  explicit HashingIStreamBuf(std::streambuf& source, HasherBase& hasher) noexcept : source_(source), hasher_(hasher) {}

  HashingIStreamBuf(const HashingIStreamBuf&) = delete;
  HashingIStreamBuf& operator=(const HashingIStreamBuf&) = delete;
};

} // namespace pep
