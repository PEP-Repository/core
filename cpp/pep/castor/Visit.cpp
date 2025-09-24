#include <pep/castor/Visit.hpp>
#include <pep/castor/Ptree.hpp>

namespace pep {
namespace castor {

const std::string Visit::RELATIVE_API_ENDPOINT = "visit";
const std::string Visit::EMBEDDED_API_NODE_NAME = "visits";

Visit::Visit(std::shared_ptr<Study> study, JsonPtr json)
  : SimpleCastorChildObject<Visit, Study>(study, json), mName(GetFromPtree<std::string>(*json, "visit_name")) {}

}
}
