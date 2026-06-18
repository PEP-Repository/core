#include <pep/castor/Form.hpp>
#include <pep/castor/Visit.hpp>
#include <pep/castor/Ptree.hpp>

namespace pep {
namespace castor {

const std::string Form::RelativeApiEndpoint = "form";
const std::string Form::EmbeddedApiNodeName = "forms";

Form::Form(std::shared_ptr<Study> study, JsonPtr json)
    : SimpleCastorChildObject<Form, Study>(study, json),
    name_(GetFromPtree<std::string>(*json, "form_name")),
    order_(GetFromPtree<int>(*json, "form_order")),
    visit_(Visit::Create(study, std::make_shared<boost::property_tree::ptree>(GetFromPtree<boost::property_tree::ptree>(*json, "_embedded.visit")))) {
}

}
}
