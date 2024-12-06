#include <pep/castor/CastorObject.hpp>

#include <boost/property_tree/json_parser.hpp>

namespace pep {
namespace castor {

const std::string CastorObject::DEFAULT_ID_FIELD = "id";

std::string CastorObject::getId() const {
  return mId;
}

}
}
