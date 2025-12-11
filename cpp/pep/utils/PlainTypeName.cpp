#include <pep/utils/PlainTypeName.hpp>
#include <boost/algorithm/string/erase.hpp>

namespace pep::detail {

std::string BoostPrettyTypeNameToPlain(std::string name) {
  boost::erase_all(name, "class ");
  boost::erase_all(name, "struct ");

  return name;
}

}
