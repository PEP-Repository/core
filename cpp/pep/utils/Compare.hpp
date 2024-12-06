#pragma once

#include <boost/algorithm/string/predicate.hpp>

namespace pep {
class CaseInsensitiveCompare {
public:
  bool operator()(const std::string& left, const std::string& right) const {
    return boost::algorithm::ilexicographical_compare(left, right);
  }
};
}
