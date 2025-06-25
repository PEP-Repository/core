#include <pep/castor/DataPoint.hpp>
#include <pep/castor/Ptree.hpp>

namespace pep
{
namespace castor
{

namespace {

std::string MakeApiRoot(std::shared_ptr<CastorObject> parent, const std::string& relative) {
  return parent->makeUrl() + "/data-points/" + relative;
}

}

const std::string DataPointBase::EMBEDDED_API_NODE_NAME = "items";

DataPointBase::DataPointBase(JsonPtr json)
    : CastorObject(json, "field_id"), mValue(GetFromPtree<std::string>(*json, "field_value")) {
}

std::string DataPointBase::GetApiRoot(std::shared_ptr<Study> study, const std::string& relative) {
  return MakeApiRoot(study, relative);
}

std::string DataPointBase::GetApiRoot(std::shared_ptr<Participant> participant, const std::string& relative) {
  return MakeApiRoot(participant, relative);
}

}
}

