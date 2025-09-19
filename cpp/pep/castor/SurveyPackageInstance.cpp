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
  mParticipantId(GetFromPtree<std::string>(*json, "participant_id")),
  mLocked(GetFromPtree<bool>(*json, "locked")),
  mProgress(GetFromPtree<int>(*json, "progress")),
  mArchived(GetFromPtree<bool>(*json, "archived")),
  mSurveyPackageId(GetFromPtree<std::string>(*json, "survey_package_id")),
  mSurveyPackageName(GetFromPtree<std::string>(*json, "survey_package_name")) {

  if (const auto& finishedOn = GetFromPtree<boost::optional<boost::property_tree::ptree>>(*json, "finished_on")) {
    mFinishedOn = MakeSharedCopy(*finishedOn);
  }
  if (const auto& sentOn = GetFromPtree<boost::optional<boost::property_tree::ptree>>(*json, "sent_on")) {
    mSentOn = MakeSharedCopy(*sentOn);
  }

  const auto& embedded = GetFromPtree<boost::optional<boost::property_tree::ptree>>(*json, "_embedded");
  if (embedded) {
    const auto& siPtrees = GetFromPtree<boost::optional<boost::property_tree::ptree>>(*embedded, "survey_instances");
    if (siPtrees) {
      mSurveyInstanceIds.reserve(siPtrees->size());
      std::transform(siPtrees->begin(), siPtrees->end(), std::back_inserter(mSurveyInstanceIds), [](const auto& keyValuePair) {
        const auto& siPtree = keyValuePair.second;
        return GetFromPtree<std::string>(siPtree, "id");
        });
    }
  }
}
const std::shared_ptr<const boost::property_tree::ptree>& SurveyPackageInstance::getFinishedOn() const {
  return mFinishedOn;
}

const std::shared_ptr<const boost::property_tree::ptree>& SurveyPackageInstance::getSentOn() const {
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
  return BulkRetrieveChildren<SurveyPackageInstance, Participant>(
    participants,
    study->makeUrl() + "/" + RELATIVE_API_ENDPOINT,
    EMBEDDED_API_NODE_NAME,
    "participant_id");
}

}
}
