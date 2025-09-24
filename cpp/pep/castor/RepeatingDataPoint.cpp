#include <pep/castor/BulkRetrieveChildren.hpp>
#include <pep/castor/RepeatingDataPoint.hpp>

namespace pep {
namespace castor {

const std::string RepeatingDataPoint::RELATIVE_API_ENDPOINT = "repeating-data-instance";

std::string RepeatingDataPoint::makeUrl() const {
  return this->getParticipant()->makeUrl() + "/data-point/repeating-data/" + this->getRepeatingDataInstance()->getId() + "/" + this->getId();
}

std::shared_ptr<Participant> RepeatingDataPoint::getParticipant() const {
  return this->getRepeatingDataInstance()->getParticipant();
}

RepeatingDataPoint::RepeatingDataPoint(std::shared_ptr<RepeatingDataInstance> repeatingDataInstance,
                                 JsonPtr json)
    : DataPoint(repeatingDataInstance, json) {}

rxcpp::observable<std::shared_ptr<RepeatingDataPoint>> RepeatingDataPoint::BulkRetrieve(std::shared_ptr<Study> study, rxcpp::observable<std::shared_ptr<RepeatingDataInstance>> rdis) {
  return BulkRetrieveChildren<RepeatingDataPoint, RepeatingDataInstance>(
    rdis,
    GetApiRoot(study, RELATIVE_API_ENDPOINT),
    EMBEDDED_API_NODE_NAME,
    "repeating_data_instance_id");
}

}
}
