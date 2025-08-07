#include <pep/castor/Form.hpp>
#include <pep/castor/Visit.hpp>
#include <pep/castor/Ptree.hpp>

namespace pep {
namespace castor {

const std::string Form::RELATIVE_API_ENDPOINT = "form";
const std::string Form::EMBEDDED_API_NODE_NAME = "forms";

Form::Form(std::shared_ptr<Study> study, JsonPtr json)
    : SimpleCastorChildObject<Form, Study>(study, json),
    mName(GetFromPtree<std::string>(*json, "form_name")),
    mOrder(GetFromPtree<int>(*json, "form_order")),
    visit(Visit::Create(study, std::make_shared<boost::property_tree::ptree>(GetFromPtree<boost::property_tree::ptree>(*json, "_embedded.visit")))) {
}

}
}
