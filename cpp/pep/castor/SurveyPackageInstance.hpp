#pragma once

#include <pep/castor/Participant.hpp>

namespace pep {
namespace castor {

class SurveyDataPoint;

//! Surveys are grouped in packages. A sent out survey package for a specific participant is a SurveyPackageInstance.
//! \remark Does not inherit from SimpleCastorChildObject<>, a.o. because there's no API endpoint to retrieve the survey package instances for a specific participant.
class SurveyPackageInstance : public ParentedCastorObject<Participant>, public SharedConstructor<SurveyPackageInstance> {
 private:
  std::string mParticipantId;
  bool mLocked;
  int mProgress;
  bool mArchived;
  std::string mSurveyPackageId;
  std::string mSurveyPackageName;
  std::shared_ptr<const boost::property_tree::ptree> mFinishedOn;
  std::shared_ptr<const boost::property_tree::ptree> mSentOn;
  std::vector<std::string> mSurveyInstanceIds;

 public:
  //! \return The ID of the participant this SurveyPackageInstance belongs to, as returned by the API. This is needed, for example, to filter SurveyPackageInstances that actually belong to a certain participant
  //! \see Participant::getSurveyPackageInstances
  std::string getParticipantId() const {
    return mParticipantId;
  }

  //! \return Whether the SurveyPackageInstance is locked. Castor should be configured to autolock a SurveyPackageInstance once it is finished
  bool isLocked() const { return mLocked; }

  //! \return The percentage of filled in form fields for this survey
  int getProgress() const { return mProgress; }

  //! \return Whether the SurveyPackageInstance is archived or not
  inline bool isArchived() const { return mArchived; }

  const std::shared_ptr<const boost::property_tree::ptree>& getFinishedOn() const;
  const std::shared_ptr<const boost::property_tree::ptree>& getSentOn() const;

  //! \return The participant this SurveyPackageInstance belongs to
  std::shared_ptr<Participant> getParticipant() const {
    return this->getParent();
  }

  //! \return A url that can be used to retrieve this SurveyPackageInstance from the Castor API
  std::string makeUrl() const override;

  std::string getSurveyPackageId() const {
    return mSurveyPackageId;
  }

  std::string getSurveyPackageName() const {
    return mSurveyPackageName;
  }

  //! \return Filled in values for this instance
  rxcpp::observable<std::shared_ptr<SurveyDataPoint>> getSurveyDataPoints();

  //! \return IDs of survey instances included in this package instance
  const std::vector<std::string>& getSurveyInstanceIds() const;

  static rxcpp::observable<std::shared_ptr<SurveyPackageInstance>> BulkRetrieve(std::shared_ptr<Study> study, rxcpp::observable<std::shared_ptr<Participant>> participants);

 protected:
  /*!
   * \brief Construct a new SurveyPackageInstance
   *
   * \param participant The Participant this SurveyPackageInstance belongs to
   * \param json The %Json response from the Castor API for this SurveyPackageInstance
   */
  SurveyPackageInstance(std::shared_ptr<Participant> participant, JsonPtr json);

 private:
  friend class SharedConstructor<SurveyPackageInstance>;
};

}
}
