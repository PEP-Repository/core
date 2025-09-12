#include <pep/castor/BulkRetrieveChildren.hpp>
#include <pep/castor/StudyDataPoint.hpp>
#include <pep/castor/Participant.hpp>

namespace pep {
namespace castor {

const std::string StudyDataPoint::RELATIVE_API_ENDPOINT = "study";

std::string StudyDataPoint::makeUrl() const {
  return this->getParticipant()->makeUrl() + "/study-data-point/" + this->getId();
}

std::shared_ptr<Participant> StudyDataPoint::getParticipant() const {
  return this->getParent();
}

rxcpp::observable<std::shared_ptr<StudyDataPoint>> StudyDataPoint::BulkRetrieve(std::shared_ptr<Study> study, rxcpp::observable<std::shared_ptr<Participant>> participants) {
  return BulkRetrieveChildren<StudyDataPoint, Participant>(
    participants,
    GetApiRoot(study, RELATIVE_API_ENDPOINT),
    "items",
    "participant_id");
}

}
}
