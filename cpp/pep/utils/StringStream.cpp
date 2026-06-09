#include <pep/utils/StringStream.hpp>

std::string_view pep::GetUnparsed(std::istringstream& ss) {
  return ss.view().substr(static_cast<std::size_t>(ss.tellg()));
}

std::string pep::GetUnparsed(std::istringstream&& ss) {
  auto offset = static_cast<std::size_t>(ss.tellg());
  return std::move(ss).str().substr(offset);
}
