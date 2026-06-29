#include <pep/castor/Site.hpp>
#include <pep/castor/Ptree.hpp>

namespace pep {
namespace castor {

const std::string Site::RelativeApiEndpoint = "site";
const std::string Site::EmbeddedApiNodeName = "sites";

Site::Site(std::shared_ptr<Study> study, JsonPtr json)
  : SimpleCastorChildObject<Site, Study>(study, json),
  abbreviation_(GetFromPtree<std::string>(*json, "abbreviation")), // Undocumented on https://data.castoredc.com/api#/site/get_study__study_id__site
  name_(GetFromPtree<std::string>(*json, "name")) { // Undocumented on https://data.castoredc.com/api#/site/get_study__study_id__site
}

std::string Site::getAbbreviation() const {
  return abbreviation_;
}

std::string Site::getName() const {
  return name_;
}

}
}
