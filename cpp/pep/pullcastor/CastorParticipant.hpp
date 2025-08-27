#pragma once

#include <pep/castor/Participant.hpp>
#include <pep/castor/RepeatingDataInstance.hpp>
#include <pep/pullcastor/StudyPuller.hpp>

namespace pep {
namespace castor {

/*!
  * \brief Caching frontend for castor::Participant instances.
  */
class CastorParticipant : public std::enable_shared_from_this<CastorParticipant>, public SharedConstructor<CastorParticipant> {
private:
  std::shared_ptr<StudyPuller> mSp;
  std::shared_ptr<Participant> mParticipant;
  std::shared_ptr<RxCache<std::shared_ptr<RepeatingDataInstance>>> mRepeatingDataInstances;

public:
  CastorParticipant(std::shared_ptr<StudyPuller> sp, std::shared_ptr<Participant> participant);

  /*!
    * \brief Produces the StudyPuller associated with this instance.
    * \return A StudyPuller instance.
    */
  inline std::shared_ptr<StudyPuller> getStudyPuller() const noexcept { return mSp; }

  /*!
    * \brief Produces the raw castor::Participant associated with this instance.
    * \return A castor::Participant instance.
    * \remark Prefer using the methods in this class over the ones in castor::Participant, since this class caches stuff.
    */
  inline std::shared_ptr<Participant> getParticipant() const noexcept { return mParticipant; }

  /*!
    * \brief Produces the castor::RepeatingDataInstance instances for this participant.
    * \return The castor::RepeatingDataInstance instances associated with this participant.
    * \remark Data are retrieved from Castor only once. Subsequent calls of this method are served from cached data.
    */
  inline rxcpp::observable<std::shared_ptr<RepeatingDataInstance>> getRepeatingDataInstances() { return mRepeatingDataInstances->observe(); }

};

}
}
