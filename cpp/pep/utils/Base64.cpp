#include <pep/utils/Base64.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/algorithm/string/replace.hpp>

namespace pep {
std::string decodeBase64URL(std::string base64) {
  // Replace -_ by +/ to support the URL encoding
  boost::replace_all(base64, "-", "+");
  boost::replace_all(base64, "_", "/");
  return std::string(boost::archive::iterators::transform_width<boost::archive::iterators::binary_from_base64<std::string::const_iterator>, 8, 6>(base64.begin()),
                     boost::archive::iterators::transform_width<boost::archive::iterators::binary_from_base64<std::string::const_iterator>, 8, 6>(base64.end()));
}

std::string encodeBase64URL(std::string data) {
  std::string base64(boost::archive::iterators::base64_from_binary<boost::archive::iterators::transform_width<std::string::const_iterator, 6, 8>>(data.begin()),
                     boost::archive::iterators::base64_from_binary<boost::archive::iterators::transform_width<std::string::const_iterator, 6, 8>>(data.end()));
  // Replace +/ by -_ support the URL encoding
  boost::replace_all(base64, "+", "-");
  boost::replace_all(base64, "/", "_");
  return base64;
}
}
