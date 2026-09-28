#include <pep/utils/StringStream.hpp>

// Note that tellg fails when the stream is at EOF on some implementations (e.g. libc++), which also throws if the stream has exceptions enabled.
// So we check for EOF first, which implies that all input has been consumed.

std::string_view pep::GetUnparsed(std::istringstream& ss) {
  if (ss.eof()) {
    return {};
  }
  return ss.view().substr(static_cast<std::size_t>(ss.tellg()));
}

std::string pep::GetUnparsed(std::istringstream&& ss) {
  if (ss.eof()) {
    return {};
  }
  auto offset = static_cast<std::size_t>(ss.tellg());
  return std::move(ss).str().substr(offset);
}
