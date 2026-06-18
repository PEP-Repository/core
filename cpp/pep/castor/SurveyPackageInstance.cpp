#include <pep/castor/BulkRetrieveChildren.hpp>
#include <pep/castor/SurveyPackageInstance.hpp>
#include <pep/castor/SurveyDataPoint.hpp>
#include <pep/castor/Ptree.hpp>

#include <entities.hpp>

namespace pep {
namespace castor {

namespace {

const std::string RELATIVE_API_ENDPOINT = "survey-package-instance";
const std::string EMBEDDED_API_NODE_NAME = "surveypackageinstance";

}

SurveyPackageInstance::SurveyPackageInstance(std::shared_ptr<Participant> participant, JsonPtr json)
  : ParentedCastorObject<Participant>(participant, json, "survey_package_instance_id"),
  participantId_(GetFromPtree<std::string>(*json, "participant_id")),
  locked_(GetFromPtree<bool>(*json, "locked")),
  progress_(GetFromPtree<int>(*json, "progress")),
  archived_(GetFromPtree<bool>(*json, "archived")),
  surveyPackageId_(GetFromPtree<std::string>(*json, "survey_package_id")),
  surveyPackageName_(GetFromPtree<std::string>(*json, "survey_package_name")) {

  if (const auto& finishedOn = GetFromPtree<boost::optional<boost::property_tree::ptree>>(*json, "finished_on")) {
    finishedOn_ = MakeSharedCopy(*finishedOn);
  }
  if (const auto& sentOn = GetFromPtree<boost::optional<boost::property_tree::ptree>>(*json, "sent_on")) {
    sentOn_ = MakeSharedCopy(*sentOn);
  }

  const auto& embedded = GetFromPtree<boost::optional<boost::property_tree::ptree>>(*json, "_embedded");
  if (embedded) {
    const auto& siPtrees = GetFromPtree<boost::optional<boost::property_tree::ptree>>(*embedded, "survey_instances");
    if (siPtrees) {
      surveyInstanceIds_.reserve(siPtrees->size());
      std::transform(siPtrees->begin(), siPtrees->end(), std::back_inserter(surveyInstanceIds_), [](const auto& keyValuePair) {
        const auto& siPtree = keyValuePair.second;
        return GetFromPtree<std::string>(siPtree, "id");
        });
    }
  }
}
const std::shared_ptr<const boost::property_tree::ptree>& SurveyPackageInstance::getFinishedOn() const {
  return finishedOn_;
}

const std::shared_ptr<const boost::property_tree::ptree>& SurveyPackageInstance::getSentOn() const {
  return sentOn_;
}

std::string SurveyPackageInstance::makeUrl() const {
  return this->getParent()->getStudy()->makeUrl() + "/" + RELATIVE_API_ENDPOINT + "/" + getId();
}

rxcpp::observable<std::shared_ptr<SurveyDataPoint>> SurveyPackageInstance::getSurveyDataPoints() {
  return SurveyDataPoint::RetrieveForParent(SharedFrom(*this));
}

const std::vector<std::string>& SurveyPackageInstance::getSurveyInstanceIds() const {
  return surveyInstanceIds_;
}

rxcpp::observable<std::shared_ptr<SurveyPackageInstance>> SurveyPackageInstance::BulkRetrieve(std::shared_ptr<Study> study, rxcpp::observable<std::shared_ptr<Participant>> participants) {
  return BulkRetrieveChildren<SurveyPackageInstance, Participant>(
    participants,
    study->makeUrl() + "/" + RELATIVE_API_ENDPOINT,
    EMBEDDED_API_NODE_NAME,
    "participant_id");
}

}
}
