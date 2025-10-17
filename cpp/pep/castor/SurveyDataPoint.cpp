#include <pep/castor/SurveyDataPoint.hpp>

#include <pep/async/RxMoveIterate.hpp>
#include <pep/castor/BulkRetrieveChildren.hpp>
#include <pep/castor/Ptree.hpp>

#include <rxcpp/operators/rx-concat_map.hpp>
#include <rxcpp/operators/rx-tap.hpp>

namespace pep {
namespace castor {

const std::string SurveyDataPoint::RELATIVE_API_ENDPOINT = "survey-package-instance";

std::string SurveyDataPoint::makeUrl() const {
  return this->getParticipant()->makeUrl() + "/data-point/survey/" + mSurveyInstanceId + "/" + this->getId();
}

std::shared_ptr<Participant> SurveyDataPoint::getParticipant() const {
  return this->getSurveyPackageInstance()->getParticipant();
}

SurveyDataPoint::SurveyDataPoint(std::shared_ptr<SurveyPackageInstance> surveyPackageInstance,
                                 JsonPtr json)
    : DataPoint(surveyPackageInstance, json), mSurveyInstanceId(GetFromPtree<std::string>(*json,"survey_instance_id")) {}

rxcpp::observable<std::shared_ptr<SurveyDataPoint>> SurveyDataPoint::BulkRetrieveFor(std::shared_ptr<CastorObject> object, rxcpp::observable<std::shared_ptr<SurveyPackageInstance>> spis) {

  // The returned JSON contains a survey instance ID but no SurveyPackageInstance ID, so we map survey instance IDs onto the SPIs they belong to
  using SpisBySurveyInstanceId = std::unordered_map<std::string, std::shared_ptr<SurveyPackageInstance>>;

  return spis // Get SPIs
    .flat_map([object](std::shared_ptr<SurveyPackageInstance> spi) { // Associate every SPI with its survey instance IDs
    return RxMoveIterate(spi->getSurveyInstanceIds())
      .map([spi](std::string id) {return std::make_pair(std::move(id), spi); });
      })
    .reduce( // Collect SPIs and survey instance IDs into a map for easy lookup
      std::make_shared<SpisBySurveyInstanceId>(),
      [](std::shared_ptr<SpisBySurveyInstanceId> map, const auto& entry) {
        [[maybe_unused]] auto emplaced = map->emplace(entry).second;
        assert(emplaced); // otherwise multiple SPIs are claiming ownership of the same survey instance ID
        return map;
      }
    )
    .concat_map([object](std::shared_ptr<SpisBySurveyInstanceId> spisBySurveyInstanceId) {
        return BulkRetrieveChildren<SurveyDataPoint, SurveyPackageInstance>(
          spisBySurveyInstanceId,
          object->makeUrl() + "/data-points/survey-instance",
          EMBEDDED_API_NODE_NAME,
          "survey_instance_id");
      });
}

rxcpp::observable<std::shared_ptr<SurveyDataPoint>> SurveyDataPoint::BulkRetrieve(std::shared_ptr<Participant> participant, rxcpp::observable<std::shared_ptr<SurveyPackageInstance>> spis) {
  return BulkRetrieveFor(participant, spis.tap([participant](std::shared_ptr<SurveyPackageInstance> spi) {assert(spi->getParticipant() == participant); }));
}

rxcpp::observable<std::shared_ptr<SurveyDataPoint>> SurveyDataPoint::BulkRetrieve(std::shared_ptr<Study> study, rxcpp::observable<std::shared_ptr<SurveyPackageInstance>> spis) {
  return BulkRetrieveFor(study, spis.tap([study](std::shared_ptr<SurveyPackageInstance> spi) {assert(spi->getParticipant()->getStudy() == study); }));
}

}
}
