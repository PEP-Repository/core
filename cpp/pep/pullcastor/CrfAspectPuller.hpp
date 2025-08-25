#pragma once

#include <pep/async/RxCache.hpp>
#include <pep/castor/Form.hpp>
#include <pep/pullcastor/StudyAspectPuller.hpp>

namespace pep {
namespace castor {

/*!
  * \brief Pulls Castor CRF (Clinical Research Form or "STUDY") data for a single Castor study.
  */
class CrfAspectPuller : public TypedStudyAspectPuller<CrfAspectPuller, CastorStudyType::STUDY>, private SharedConstructor<CrfAspectPuller> {
  friend class StudyAspectPuller;
  friend class SharedConstructor<CrfAspectPuller>;

private:
  class FormPuller;
  using FieldValues = std::vector<std::shared_ptr<FieldValue>>;
  using FormPullersByFormId = std::unordered_map<std::string, std::shared_ptr<FormPuller>>;

  using StudyDataPoints = std::vector<std::shared_ptr<StudyDataPoint>>;
  using StudyDataPointsByParticipant = std::unordered_map<std::shared_ptr<Participant>, std::shared_ptr<StudyDataPoints>>;

  bool mImmediatePartialData;
  std::shared_ptr<RxCache<std::shared_ptr<FormPullersByFormId>>> mFormPullers;
  std::shared_ptr<RxCache<std::shared_ptr<StudyDataPointsByParticipant>>> mSdpsByParticipant;

  CrfAspectPuller(std::shared_ptr<StudyPuller> study, const StudyAspect& aspect);

  rxcpp::observable<std::shared_ptr<StudyDataPoint>> getStudyDataPoints(std::shared_ptr<Participant> participant);
  rxcpp::observable<std::shared_ptr<StorableColumnContent>> loadFormContentFromCastor(std::shared_ptr<CastorParticipant> participant, const std::string& formId, std::shared_ptr<FieldValues> dps);

public:
  /*!
  * \brief Produces (an observable emitting) the Castor content to store for the specified participant.
  * \param participant The CastorParticipant instance representing the participant to process.
  * \return Entries representing the Castor CRF data that should be stored in PEP for the specified Castor participant.
  */
  rxcpp::observable<std::shared_ptr<StorableColumnContent>> getStorableContent(std::shared_ptr<CastorParticipant> participant) override;
};

}
}
