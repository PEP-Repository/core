#pragma once

#include <pep/castor/DataPoint.hpp>
#include <pep/castor/SurveyPackageInstance.hpp>

namespace pep {
namespace castor {

class SurveyDataPoint : public DataPoint<SurveyDataPoint, SurveyPackageInstance>, public SharedConstructor<SurveyDataPoint> {
 private:
  std::string mSurveyInstanceId;

  /* !
   * \brief Implementor for the two public "BulkRetrieve" methods.
   *
   * \param object The (Study or a Participant) object whose API will be accessed to retrieve survey data points.
   * \param spis The SurveyPackageInstance objects that can serve as the parent objects for bulk-retrieved SurveyDataPoint instances.
   * 
   * \return SurveyDataPoint instances produced by the API that could be assigned to one of the parent SurveyPackageInstance objects.
   */
  static rxcpp::observable<std::shared_ptr<SurveyDataPoint>> BulkRetrieveFor(std::shared_ptr<CastorObject> object, rxcpp::observable<std::shared_ptr<SurveyPackageInstance>> spis);

 public:
  static const std::string RELATIVE_API_ENDPOINT;

  std::string makeUrl() const override;

  std::shared_ptr<Participant> getParticipant() const override;
  std::shared_ptr<SurveyPackageInstance> getSurveyPackageInstance() const { return this->getParent(); }

  DataPointType getType() const override { return SURVEY; }

  static rxcpp::observable<std::shared_ptr<SurveyDataPoint>> BulkRetrieve(std::shared_ptr<Study> study, rxcpp::observable<std::shared_ptr<SurveyPackageInstance>> spis);
  static rxcpp::observable<std::shared_ptr<SurveyDataPoint>> BulkRetrieve(std::shared_ptr<Participant> participant, rxcpp::observable<std::shared_ptr<SurveyPackageInstance>> spis);

 protected:
  /*!
   * \brief Construct a new SurveyDataPoint
   *
   * \param surveyPackageInstance The SurveyPackageInstance this SurveyDataPoint belongs to
   * \param json The %Json response from the Castor API for this SurveyDataPoint
   */
  SurveyDataPoint(std::shared_ptr<SurveyPackageInstance> surveyPackageInstance, JsonPtr json);

  friend class SharedConstructor<SurveyDataPoint>;
};

}
}
