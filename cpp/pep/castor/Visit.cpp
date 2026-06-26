#include <pep/castor/Visit.hpp>
#include <pep/castor/Ptree.hpp>

namespace pep {
namespace castor {

const std::string Visit::RelativeApiEndpoint = "visit";
const std::string Visit::EmbeddedApiNodeName = "visits";

Visit::Visit(std::shared_ptr<Study> study, JsonPtr json)
  : SimpleCastorChildObject<Visit, Study>(study, json), name_(GetFromPtree<std::string>(*json, "visit_name")) {}

}
}
