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
  boost::optional<boost::property_tree::ptree> mFinishedOn;
  boost::optional<boost::property_tree::ptree> mSentOn;
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

  const boost::optional<boost::property_tree::ptree>& getFinishedOn() const;
  const boost::optional<boost::property_tree::ptree>& getSentOn() const;

  //! \return The participant this SurveyPackageInstance belongs to
  const std::shared_ptr<Participant> getParticipant() const {
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
  SurveyPackageInstance(std::shared_ptr<Participant> participant, JsonPtr json)
      : ParentedCastorObject<Participant>(participant, json, "survey_package_instance_id"),
    mParticipantId(GetFromPtree<std::string>(*json, "participant_id")),
    mLocked(GetFromPtree<bool>(*json, "locked")),
    mProgress(GetFromPtree<int>(*json, "progress")),
    mArchived(GetFromPtree<bool>(*json, "archived")),
    mSurveyPackageId(GetFromPtree<std::string>(*json, "survey_package_id")),
    mSurveyPackageName(GetFromPtree<std::string>(*json, "survey_package_name")) {

    if (const auto& finishedOn = GetFromPtree<boost::optional<boost::property_tree::ptree>>(*json, "finished_on")) {
      mFinishedOn = *finishedOn;
    }
    if (const auto& sentOn = GetFromPtree<boost::optional<boost::property_tree::ptree>>(*json, "sent_on")) {
      mSentOn = *sentOn;
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

 private:
  friend class SharedConstructor<SurveyPackageInstance>;
};

}
}
