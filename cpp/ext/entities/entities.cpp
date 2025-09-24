#include "entities.hpp"
#include <vector>

extern "C" size_t decode_html_entities_utf8(char *dest, const char *src);

std::string decode_html_entities_utf8(const std::string& source) {
  std::vector<char> keeper(source.length() + 1, '\0');
  char* decoded = keeper.data();
  size_t size = decode_html_entities_utf8(decoded, source.c_str());
  return std::string(decoded, size);
}

