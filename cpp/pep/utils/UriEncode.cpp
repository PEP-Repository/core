#include <pep/utils/UriEncode.hpp>
#include <sstream>
#include <iomanip>

#include <boost/algorithm/hex.hpp>

namespace {

void UriEncodeLetter(const char c, bool encodeSlash,
      std::ostringstream& out)
{
  if ( ('A'<=c && c<='Z') || ('a'<=c && c<='z')
      || ('0'<=c && c<='9') || c=='_' || c=='-' || c=='~' || c=='.'
      || (c=='/' && !encodeSlash)) {
    out << c;
  } else {
    // To write the underlying 'byte-value' of c, we need to
    // reinterpret it first as an uint8_t via *reinterpret_cast<uint8_t*>(&c).
    // Since << interprets an uint8_t as a character (and not as a number)
    // we finally cast it to an int.
    out << "%" << std::setw(2) << static_cast<int>(*reinterpret_cast<const uint8_t*>(&c));
    // N.B. Since out << std::hex << std::uppercase << set::setfill('0')
    // has been called in UriEncode(...), this adds one of
    //   00, 01, 02, ..., 0F, 10, ... FF
    // to out.
  }
}

} // namespace

namespace pep
{
  std::string UriEncode(const std::string& input, bool encodeSlash) {
    std::ostringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0');

    for (const char c : input) {
      UriEncodeLetter(c, encodeSlash, ss);
    }

    return ss.str();
  }


  std::string UriDecode(const std::string& input, bool plusAsSpace) {
    std::string str;
    str.reserve(input.length());

    for (size_t i = 0; i < input.length(); i++) {
      if (plusAsSpace && input[i] == '+') {
        str += ' ';
        continue;
      }
      if (input[i] != '%') {
        str += input[i];
        continue;
      }

      // We've got "%XY"
      if (input.length() - i < 3) {
        // We found "%", but not both "X" and "Y".
        throw std::runtime_error("badly URI-encoded string");
      }

      str += boost::algorithm::unhex(input.substr(i+1,2));

      // Skip over the two (additional) characters that we already processed.
      i += 2U;
    }

    return str;
  }

}
