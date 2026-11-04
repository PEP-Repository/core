#include <pep/castor/CastorObject.hpp>
#include <pep/castor/Ptree.hpp>
#include <pep/castor/CastorConnection.hpp> // for castor::JsonPtr

#include <string>

namespace pep {
namespace castor {

const std::string CastorObject::DefaultIdField = "id";

CastorObject::CastorObject(JsonPtr json, const std::string& idField)
  : id_(GetFromPtree<std::string>(*json, idField)) {}

std::string CastorObject::getId() const {
  return id_;
}

}
}
