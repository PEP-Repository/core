#pragma once

#include <pep/pullcastor/RepeatingDataPuller.hpp>
#include <pep/pullcastor/StudyAspectPuller.hpp>

namespace pep {
namespace castor {

/*!
  * \brief Pulls Castor repeating data ("REPEATING_DATA") for a single Castor study.
  */
class RepeatingDataAspectPuller : public TypedStudyAspectPuller<RepeatingDataAspectPuller, CastorStudyType::REPEATING_DATA>, private SharedConstructor<RepeatingDataAspectPuller> {
  friend class StudyAspectPuller;
  friend class SharedConstructor<RepeatingDataAspectPuller>;

private:
  std::shared_ptr<RxCache<std::shared_ptr<std::vector<std::shared_ptr<RepeatingDataPuller>>>>> mRepeatingDataPullers;

  RepeatingDataAspectPuller(std::shared_ptr<StudyPuller> study, const StudyAspect& aspect);

public:
  /*!
  * \brief Produces (an observable emitting) the Castor content to store for the specified participant.
  * \param participant The CastorParticipant instance representing the participant to process.
  * \return Entries representing the Castor repeating data that should be stored in PEP for the specified Castor participant.
  */
  rxcpp::observable<std::shared_ptr<StorableColumnContent>> getStorableContent(std::shared_ptr<CastorParticipant> participant) override;
};

}
}
