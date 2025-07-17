#pragma once

#include <pep/castor/CastorObject.hpp>
#include <pep/async/WaitGroup.hpp>

namespace pep {
namespace castor {

class Site;
class Participant;
class Form;
class OptionGroup;
class Field;
class Visit;
class Survey;
class SurveyPackage;
class SurveyPackageInstance;
class RepeatingData;

//! A study in Castor
class Study : public CastorObject, public SharedConstructor<Study> {
 private:
  std::string mName;
  std::string mSlug;

 public:
  std::shared_ptr<CastorConnection> getConnection() const override { return mConnection; }

  /*!
   * \brief Create a participant in this study.
   *
   * The site of the participant will be the default site of this study.
   *
   * \param participantId The ID (short pseudonym) for this participant
   * \return An observable that, if no error occurs, emits the created participant
   */
  rxcpp::observable<std::shared_ptr<Participant>> createParticipant(const std::string& participantId);

  //! \return An observable that, if no error occurs, emits a Participant for all participants in this study
  rxcpp::observable<std::shared_ptr<Participant>> getParticipants();

  //! \return An observable that, if no error occurs, emits a SurveyPackageInstance for all survey package instances in this study
  rxcpp::observable<std::shared_ptr<SurveyPackageInstance>> getSurveyPackageInstances();

  //! \return An observable that, if no error occurs, emits a SurveyPackage for all survey packages in this study
  rxcpp::observable<std::shared_ptr<SurveyPackage>> getSurveyPackages();

  //! \return An observable that, if no error occurs, emits a Survey for all surveys in this study
  rxcpp::observable<std::shared_ptr<Survey>> getSurveys();

  //! \return An observable that, if no error occurs, emits a RepeatingData for all repeating data (definitions) in this study
  rxcpp::observable<std::shared_ptr<RepeatingData>> getRepeatingData();

  //! \return An observable that, if no error occurs, emits a Site for all sites in this study
  rxcpp::observable<std::shared_ptr<Site>> getSites();

  rxcpp::observable<std::shared_ptr<OptionGroup>> getOptionGroups();

  rxcpp::observable<std::shared_ptr<Field>> getFields();

  /*!
   * \brief Set the default site for this study
   *
   * \param abbreviation The abbreviation of the site
   */
  void setDefaultSiteByAbbreviation(const std::string& abbreviation);

  //! \return A url that can be used to retrieve this study from the Castor API
  std::string makeUrl() const override;

  static rxcpp::observable<std::shared_ptr<Study>> RetrieveForParent(std::shared_ptr<CastorConnection> connection);

  //! \return The full name of the study
  std::string getName() const;
  //! \return The slug of the study to get. In the study settings in Castor this is called Study ID
  std::string getSlug() const;

  //! \return Observable that, if no eror occurs, emits all forms of this study.
  rxcpp::observable<std::shared_ptr<Form>> getForms();

  //! \return Observable that, if no eror occurs, emits all visits of this study.
  rxcpp::observable<std::shared_ptr<Visit>> getVisits();

 protected:
  /*!
   * \brief Construct a new Study
   * \param connection The CastorConnection to use for API requests related to this study
   * \param json The %Json response from the Castor API for this study
   */
  Study(std::shared_ptr<CastorConnection> connection, JsonPtr json);

 private:
  std::shared_ptr<CastorConnection> mConnection;
  std::shared_ptr<std::string> mdefaultSiteId;
  std::shared_ptr<WaitGroup> mdefaultSiteWg = WaitGroup::Create();

  //! \return Observable that, if no error occurs, emits the ID of the default site of the study. This will not emit any updates to the default site
  rxcpp::observable<std::string> getDefaultSiteId() const;

  friend class SharedConstructor<Study>;
};

}
}
