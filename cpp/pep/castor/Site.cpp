#include <pep/castor/Site.hpp>
#include <pep/castor/Ptree.hpp>

namespace pep {
namespace castor {

const std::string Site::RELATIVE_API_ENDPOINT = "site";
const std::string Site::EMBEDDED_API_NODE_NAME = "sites";

Site::Site(std::shared_ptr<Study> study, JsonPtr json)
  : SimpleCastorChildObject<Site, Study>(study, json),
  mAbbreviation(GetFromPtree<std::string>(*json, "abbreviation")), // Undocumented on https://data.castoredc.com/api#/site/get_study__study_id__site
  mName(GetFromPtree<std::string>(*json, "name")) { // Undocumented on https://data.castoredc.com/api#/site/get_study__study_id__site
}

std::string Site::getAbbreviation() const {
  return mAbbreviation;
}

std::string Site::getName() const {
  return mName;
}

}
}
