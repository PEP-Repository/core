#include <pep/utils/MiscUtil.hpp>

#include <boost/property_tree/ptree.hpp>

namespace pep {

std::string BoolToString(bool value) {
  return value ? "true" : "false";
}

bool StringToBool(const std::string& value) {
  return value == "true";
}

boost::property_tree::path RawPtreePath(const std::string& path) {
  return {path, '\0'};
}

} // namespace pep
