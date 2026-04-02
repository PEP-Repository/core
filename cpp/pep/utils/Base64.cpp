#include <pep/utils/Base64.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/algorithm/string/replace.hpp>

#include <string_view>

using namespace boost::archive::iterators;

namespace pep {
std::string DecodeBase64Url(std::string base64) {
  // Replace -_ by +/ to support the URL encoding
  boost::replace_all(base64, "-", "+");
  boost::replace_all(base64, "_", "/");
  using base64_decode_it = transform_width<binary_from_base64<std::string::const_iterator>, 8, 6>;
  return std::string(base64_decode_it(base64.cbegin()), base64_decode_it(base64.cend()));
}

std::string EncodeBase64Url(std::string_view data) {
  using base64_encode_it = base64_from_binary<transform_width<std::string_view::iterator, 6, 8>>;
  std::string base64(base64_encode_it(data.begin()), base64_encode_it(data.end()));
  // Replace +/ by -_ support the URL encoding
  boost::replace_all(base64, "+", "-");
  boost::replace_all(base64, "/", "_");
  return base64;
}
}
