#include <pep/castor/SurveyPackageInstance.hpp>
#include <pep/castor/SurveyDataPoint.hpp>

#include <entities.hpp>

namespace pep {
namespace castor {

namespace {

const std::string RELATIVE_API_ENDPOINT = "survey-package-instance";
const std::string EMBEDDED_API_NODE_NAME = "surveypackageinstance";

}

const boost::optional<boost::property_tree::ptree>& SurveyPackageInstance::getFinishedOn() const {
  return mFinishedOn;
}

const boost::optional<boost::property_tree::ptree>& SurveyPackageInstance::getSentOn() const {
  return mSentOn;
}

std::string SurveyPackageInstance::makeUrl() const {
  return this->getParent()->getStudy()->makeUrl() + "/" + RELATIVE_API_ENDPOINT + "/" + getId();
}

rxcpp::observable<std::shared_ptr<SurveyDataPoint>> SurveyPackageInstance::getSurveyDataPoints() {
  return SurveyDataPoint::RetrieveForParent(SharedFrom(*this));
}

const std::vector<std::string>& SurveyPackageInstance::getSurveyInstanceIds() const {
  return mSurveyInstanceIds;
}

rxcpp::observable<std::shared_ptr<SurveyPackageInstance>> SurveyPackageInstance::BulkRetrieve(std::shared_ptr<Study> study, rxcpp::observable<std::shared_ptr<Participant>> participants) {
  return CastorObject::BulkRetrieveList<SurveyPackageInstance, Participant>(
    participants,
    study->makeUrl() + "/" + RELATIVE_API_ENDPOINT,
    EMBEDDED_API_NODE_NAME,
    "participant_id");
}

}
}
