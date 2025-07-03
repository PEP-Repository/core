#pragma once

#include <pep/castor/Study.hpp>
#include <pep/pullcastor/EnvironmentPuller.hpp>
#include <pep/pullcastor/FieldValue.hpp>
#include <pep/pullcastor/PullCastorUtils.hpp>
#include <pep/pullcastor/RepeatingDataPuller.hpp>

namespace pep {
namespace castor {

/*!
  * \brief Produces Castor data for a single Castor study.
  * \remark Ensures that Castor study data are only loaded once, even if multiple aspects are imported from that study.
  */
class StudyPuller : public std::enable_shared_from_this<StudyPuller>, private SharedConstructor<StudyPuller> {
  friend class SharedConstructor<StudyPuller>;

private:
  using FieldsById = std::unordered_map<std::string, std::shared_ptr<Field>>;
  using RepeatingDataPullersById = std::unordered_map<std::string, std::shared_ptr<RepeatingDataPuller>>;

  std::shared_ptr<EnvironmentPuller> mEnvironment;
  std::shared_ptr<Study> mStudy;
  std::shared_ptr<std::vector<StudyAspect>> mAspects;

  std::shared_ptr<RxCache<std::shared_ptr<Participant>>> mParticipants;
  std::shared_ptr<RxCache<std::shared_ptr<Field>>> mFields;
  std::shared_ptr<RxCache<std::shared_ptr<FieldsById>>> mFieldsById;
  std::shared_ptr<RxCache<std::shared_ptr<RepeatingDataPullersById>>> mRepeatingDataPullers;

  std::shared_ptr<RxCache<std::shared_ptr<RepeatingDataInstance>>> mRepeatingDataInstances;
  std::shared_ptr<RxCache<std::shared_ptr<RepeatingDataPoint>>> mRepeatingDataPoints;

  explicit StudyPuller(std::shared_ptr<EnvironmentPuller> environment, std::shared_ptr<Study> study, std::shared_ptr<std::vector<StudyAspect>> aspects);

public:
  /*!
    * \brief Produces StudyPuller instances for the specified (PEP+Castor) environment.
    * \param environment The EnvironmentPuller for the PEP+Castor environment we'll import data from.
    * \return (An observable emitting) StudyPuller instances for every Castor study from which data needs to be imported.
    */
  static rxcpp::observable<std::shared_ptr<StudyPuller>> CreateChildrenFor(std::shared_ptr<EnvironmentPuller> environment);

  /*!
    * \brief Produces the Castor data that should be stored in PEP for this study.
    * \return (An observable emitting) StorableCellContent instances specifying the data that PEP should be storing.
    */
  rxcpp::observable<std::shared_ptr<StorableCellContent>> getStorableContent();

  /*!
    * \brief Produces the EnvironmentPuller associated with this instance.
    * \return An EnvironmentPuller instance.
    */
  inline std::shared_ptr<EnvironmentPuller> getEnvironmentPuller() const noexcept { return mEnvironment; }

  /*!
    * \brief Produces the aspects to pull for this study.
    * \return StudyAspect instances.
    */
  inline std::shared_ptr<std::vector<StudyAspect>> getAspects() const noexcept { return mAspects; }

  /*!
    * \brief Produces the castor::Study instance for this study.
    * \return A castor::Study instance.
    * \remark Prefer using the methods in this class over the ones in castor::Study, since this class caches stuff.
    */
  inline std::shared_ptr<Study> getStudy() const noexcept { return mStudy; }

  /*!
    * \brief Produces the castor::Participant instances to process for this study.
    * \return The castor::Participant instances associated with this study.
    * \remark Data are retrieved from Castor only once. Subsequent calls of this method are served from cached data.
    */
  inline rxcpp::observable<std::shared_ptr<Participant>> getParticipants() { return mParticipants->observe(); }

  /*!
    * \brief Produces the castor::RepeatingDataInstance instances for the specified participant.
    * \return The castor::RepeatingDataInstance instances associated with the participant.
    * \remark When importing all of this study's records, data are bulk retrieved from Castor (once) and served from cached data.
    */
  rxcpp::observable<std::shared_ptr<RepeatingDataInstance>> getRepeatingDataInstances(std::shared_ptr<Participant> participant);

  /*!
    * \brief Produces the castor::RepeatingDataPoint instances for the specified repeating data instance.
    * \return The castor::RepeatingDataPoint instances associated with the repeating data instance.
    * \remark When importing all of this study's records, data are bulk retrieved from Castor (once) and served from cached data.
    */
  rxcpp::observable<std::shared_ptr<RepeatingDataPoint>> getRepeatingDataPoints(std::shared_ptr<RepeatingDataInstance> rdi);

  /*!
    * \brief Produces the castor::Field instances for this study.
    * \return The castor::Field instances associated with this study.
    * \remark Data are retrieved from Castor only once. Subsequent calls of this method are served from cached data.
    */
  rxcpp::observable<std::shared_ptr<Field>> getFields();

  /*!
    * \brief Produces RepeatingDataPuller instances for every RepeatingData (type) in this study.
    * \return The RepeatingDataPuller instances associated with this study.
    * \remark Data are retrieved from Castor only once. Subsequent calls of this method are served from cached data.
  */
  rxcpp::observable<std::shared_ptr<RepeatingDataPuller>> getRepeatingDataPullers();

  /*!
    * \brief Produces the RepeatingDataPuller instance associated with the specified RepeatingData (definition) ID.
    * \return The RepeatingDataPuller instance associated with the ID.
    * \remark Data are retrieved from Castor only once. Subsequent calls of this method are served from cached data.
    */
  rxcpp::observable<std::shared_ptr<RepeatingDataPuller>> getRepeatingDataPuller(const std::string& repeatingDataId);

  /*!
    * \brief Converts a DataPointBase to a FieldValue (coupling it with the appropriate Field instance).
    * \param dp The DataPointBase to convert.
    * \return (An observable emitting) a FieldValue for the specified DataPointBase.
    */
  rxcpp::observable<std::shared_ptr<FieldValue>> toFieldValue(std::shared_ptr<DataPointBase> dp);
};

}
}
