#pragma once

#include <string>

namespace pep
{
  // The URI encoding variation canonized by Amazon to use with S3.
  std::string UriEncode(const std::string& input, bool encodeSlash=true);
  std::string UriDecode(const std::string& input, bool plusAsSpace=false);
}
