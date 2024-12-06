#include <pep/castor/Site.hpp>

namespace pep {
namespace castor {

const std::string Site::RELATIVE_API_ENDPOINT = "site";
const std::string Site::EMBEDDED_API_NODE_NAME = "sites";

std::string Site::getAbbreviation() const {
  return mAbbreviation;
}

std::string Site::getName() const {
  return mName;
}

}
}
