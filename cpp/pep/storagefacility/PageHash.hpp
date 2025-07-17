#pragma once

#include <pep/utils/Md5.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string/case_conv.hpp>

namespace pep
{
template <typename... Args>
std::string ETag(Args&&... pieces) {
  return "\"" +
    boost::algorithm::to_lower_copy(boost::algorithm::hex(
          Md5().digest(std::forward<Args>(pieces)...)))
      + "\"";
}

std::string XXHashStr(const std::string& data);
std::string PageHash(const std::string& data);
}

