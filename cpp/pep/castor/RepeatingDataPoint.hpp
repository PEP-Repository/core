#pragma once

#include <pep/castor/CastorConnection.hpp> // for castor::JsonPtr
#include <pep/castor/DataPoint.hpp>
#include <pep/castor/Participant.hpp>
#include <pep/castor/RepeatingDataInstance.hpp>
#include <pep/castor/Study.hpp>
#include <pep/utils/Shared.hpp>

#include <memory>
#include <string>

namespace pep {
namespace castor {

class RepeatingDataPoint : public DataPoint<RepeatingDataPoint, RepeatingDataInstance>, public SharedConstructor<RepeatingDataPoint> {

 public:
  static const std::string RelativeApiEndpoint;

  std::string makeUrl() const override;

  std::shared_ptr<RepeatingDataInstance> getRepeatingDataInstance() const { return this->getParent(); }
  std::shared_ptr<Participant> getParticipant() const override;

  DataPointType getType() const override { return DataPointType::Repeating; }

  static rxcpp::observable<std::shared_ptr<RepeatingDataPoint>> BulkRetrieve(std::shared_ptr<Study> study, rxcpp::observable<std::shared_ptr<RepeatingDataInstance>> rdis);

 protected:
  /// \brief Construct a new DataPoint
  ///
  /// \param repeatingDataInstance The RepeatingDataInstance this data point belongs to
  /// \param json The %Json response from the Castor API for this data point
  RepeatingDataPoint(std::shared_ptr<RepeatingDataInstance> repeatingDataInstance, JsonPtr json);

  friend class SharedConstructor<RepeatingDataPoint>;
};

}
}
