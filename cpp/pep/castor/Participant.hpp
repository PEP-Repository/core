#pragma once

#include <pep/castor/Study.hpp>

namespace pep {
namespace castor {

class StudyDataPoint;
class SurveyDataPoint;
class SurveyPackageInstance;
class RepeatingDataInstance;

//! A participant in a Castor Study
class Participant : public SimpleCastorChildObject<Participant, Study>, public SharedConstructor<Participant> {
 private:
  int mProgress;
  std::string mStatus;
  std::unique_ptr<boost::property_tree::ptree> mUpdatedOn;

 public:
  static const std::string RELATIVE_API_ENDPOINT;
  static const std::string EMBEDDED_API_NODE_NAME;

  /*!
   * \brief Creates a new participant in Castor
   *
   * \param study The study to create the participant in.
   * \param participantId The ID for the participant to create.
   * \param siteId The site ID for the participant to create.
   */
  static rxcpp::observable<std::shared_ptr<Participant>> CreateNew(std::shared_ptr<Study> study, const std::string& participantId, const std::string& siteId);

  //! \return The entered form values for this participant
  rxcpp::observable<std::shared_ptr<StudyDataPoint>> getStudyDataPoints();

  rxcpp::observable<std::shared_ptr<RepeatingDataInstance>> getRepeatingDataInstances();

  //! \return The percentage of filled in form fields for this participant
  int getProgress() const { return mProgress; }

  std::shared_ptr<Study> getStudy() const { return this->getParent(); }

  const boost::property_tree::ptree& getUpdatedOn() const;

  std::string getStatus() const { return mStatus; }

  bool isLocked() const { return this->getStatus() == "locked"; }

 protected:
  /*!
   * \brief Construct a new Participant
   *
   * \param study The Study this participant belongs to
   * \param json The %Json response from the Castor API for this participant
   */
  Participant(std::shared_ptr<Study> study, JsonPtr json);

 private:
  friend class SharedConstructor<Participant>;
};

}
}
