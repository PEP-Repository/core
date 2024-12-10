#include <pep/castor/Participant.hpp>
#include <pep/castor/StudyDataPoint.hpp>
#include <pep/castor/SurveyDataPoint.hpp>
#include <pep/castor/SurveyPackageInstance.hpp>
#include <pep/castor/RepeatingDataInstance.hpp>

namespace pep {
namespace castor {

const std::string Participant::RELATIVE_API_ENDPOINT = "participant";
const std::string Participant::EMBEDDED_API_NODE_NAME = "participants";

rxcpp::observable<std::shared_ptr<Participant>> Participant::CreateNew(std::shared_ptr<Study> study, const std::string& participantId, const std::string& siteId) {
  auto connection = study->getConnection();
  auto request = connection->makePost(study->makeUrl() + "/" + RELATIVE_API_ENDPOINT,
    "{\"participant_id\": \"" + participantId + "\",\"site_id\": \"" + siteId + "\"}");
  return connection->sendCastorRequest(request).map([study](JsonPtr response) {
    return Participant::Create(study, response);
    });
}

rxcpp::observable<std::shared_ptr<StudyDataPoint>> Participant::getStudyDataPoints() {
  return StudyDataPoint::RetrieveForParent(SharedFrom(*this));
}

rxcpp::observable<std::shared_ptr<RepeatingDataInstance>> Participant::getRepeatingDataInstances() {
  return RepeatingDataInstance::RetrieveForParent(SharedFrom(*this))
    .on_error_resume_next(&RepeatingDataInstance::ConvertNotFoundToEmpty);
}

const boost::property_tree::ptree& Participant::getUpdatedOn() const {
  return mUpdatedOn;
}

}
}
