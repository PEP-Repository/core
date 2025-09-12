#include <pep/castor/CastorObject.hpp>
#include <pep/castor/Ptree.hpp>

namespace pep {
namespace castor {

const std::string CastorObject::DEFAULT_ID_FIELD = "id";

CastorObject::CastorObject(JsonPtr json, const std::string& idField)
  : mId(GetFromPtree<std::string>(*json, idField)) {}

std::string CastorObject::getId() const {
  return mId;
}

}
}
