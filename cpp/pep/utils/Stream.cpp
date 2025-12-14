#include <pep/utils/Log.hpp>
#include <pep/utils/Stream.hpp>

#ifdef _WIN32
# include <io.h>
# include <fcntl.h>
#endif

namespace pep {

#ifdef _WIN32

namespace {

/// \param file Affected file
/// \param stream Corresponding C++ stream
/// \param mode E.g. \c _O_BINARY (fcntl.h)
/// \return The file's previous mode
int SetMode(FILE* file, std::streambuf* stream, int mode) {
  assert(file && stream);
  // Flush / sync
  if (stream->pubsync() == -1) {
    throw std::runtime_error("Could not sync stream");
  }
  if (auto ret = ::_setmode(_fileno(file), mode); ret != -1) {
    return ret; // previous mode
  } else {
    throw std::system_error(errno, std::generic_category(), "Could not set mode on file");
  }
}

}

SetBinaryFileMode::SetBinaryFileMode(FILE* file, std::streambuf* stream) : file_{ file }, stream_{ stream } {
  prevMode_ = SetMode(file_, stream_, _O_BINARY);
}

SetBinaryFileMode::SetBinaryFileMode(SetBinaryFileMode&& other) noexcept
  : file_{ std::exchange(other.file_, {}) },
  stream_{ std::exchange(other.stream_, {}) },
  prevMode_{ other.prevMode_ } {
}

SetBinaryFileMode::~SetBinaryFileMode() noexcept {
  if (!file_) { return; }
  try {
    SetMode(file_, stream_, prevMode_);
    file_ = {};
  } catch (const std::system_error& ex) {
    LOG("File mode", warning) << ex.what();
  }
}

SetBinaryFileMode SetBinaryFileMode::ForStdin() {
  return SetBinaryFileMode(stdin, std::cin.rdbuf());
}

SetBinaryFileMode SetBinaryFileMode::ForStdout() {
  return SetBinaryFileMode(stdout, std::cout.rdbuf());
}

#else // !_WIN32

SetBinaryFileMode::SetBinaryFileMode(SetBinaryFileMode&& other) noexcept = default;
SetBinaryFileMode::~SetBinaryFileMode() noexcept = default;

SetBinaryFileMode SetBinaryFileMode::ForStdin() { return SetBinaryFileMode(); }
SetBinaryFileMode SetBinaryFileMode::ForStdout() { return SetBinaryFileMode(); }

#endif

}
