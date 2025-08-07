#include <pep/castor/BulkRetrieveChildren.hpp>
#include <pep/castor/RepeatingDataInstance.hpp>
#include <pep/castor/RepeatingDataPoint.hpp>
#include <pep/castor/RepeatingData.hpp>
#include <pep/castor/Ptree.hpp>

namespace pep {
namespace castor {

const std::string RepeatingDataInstance::RELATIVE_API_ENDPOINT = "repeating-data-instance";
const std::string RepeatingDataInstance::EMBEDDED_API_NODE_NAME = "repeatingDataInstance"; // Documented as "repeatingDataInstances" on https://data.castoredc.com/api#/repeating-data-instance/get_study__study_id__participant__participant_id__repeating_data_instance

RepeatingDataInstance::RepeatingDataInstance(std::shared_ptr<Participant> participant, JsonPtr json)
  : SimpleCastorChildObject<RepeatingDataInstance, Participant>(participant, json),
  mParticipantId(GetFromPtree<std::string>(*json, "participant_id")),
  mName(GetFromPtree<std::string>(*json, "name")),
  mArchived(GetFromPtree<bool>(*json, "archived")),
  repeatingData(RepeatingData::Create(participant->getStudy(), std::make_shared<boost::property_tree::ptree>(GetFromPtree<boost::property_tree::ptree>(*json, "_embedded.repeating_data")))) // Node name "repeating_data" differs from RepeatingData::EMBEDDED_API_NODE_NAME (defined to "repeatingData")
  {
}

rxcpp::observable<std::shared_ptr<RepeatingDataPoint>> RepeatingDataInstance::getRepeatingDataPoints() {
  return RepeatingDataPoint::RetrieveForParent(SharedFrom(*this));
}

rxcpp::observable<std::shared_ptr<RepeatingDataInstance>> RepeatingDataInstance::BulkRetrieve(std::shared_ptr<Study> study, rxcpp::observable<std::shared_ptr<Participant>> participants) {
  return BulkRetrieveChildren<RepeatingDataInstance, Participant>(
    participants,
    study->makeUrl() + "/" + RELATIVE_API_ENDPOINT,
    EMBEDDED_API_NODE_NAME,
    "participant_id");
}

rxcpp::observable<std::shared_ptr<RepeatingDataInstance>> RepeatingDataInstance::ConvertNotFoundToEmpty(std::exception_ptr ep) {
  try {
    std::rethrow_exception(ep);
  }
  catch (const castor::CastorException& ex) {
    if (ex.status == castor::CastorConnection::NOT_FOUND) {
      return rxcpp::rxs::empty<std::shared_ptr<RepeatingDataInstance>>();
    }
    throw;
  }
}

}
}
