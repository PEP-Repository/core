#pragma once

#include <pep/castor/DataPoint.hpp>

namespace pep {
namespace castor {

class StudyDataPoint : public DataPoint<StudyDataPoint, Participant>, public SharedConstructor<StudyDataPoint> {

 public:
  static const std::string RELATIVE_API_ENDPOINT;

  std::string makeUrl() const override;

  std::shared_ptr<Participant> getParticipant() const override;

  DataPointType getType() const override { return STUDY; }

  static rxcpp::observable<std::shared_ptr<StudyDataPoint>> BulkRetrieve(std::shared_ptr<Study> study, rxcpp::observable<std::shared_ptr<Participant>> participants);

 protected:
  /*!
   * \brief Construct a new StudyDataPoint
   *
   * \param participant The Participant this StudyDataPoint belongs to
   * \param json The %Json response from the Castor API for this StudyDataPoint
   */
  StudyDataPoint(std::shared_ptr<Participant> participant, JsonPtr json)
      : DataPoint(participant, json) {}

  friend class SharedConstructor<StudyDataPoint>;
};

}
}
