#include <pep/pullcastor/CastorParticipant.hpp>

namespace pep {
namespace castor {

CastorParticipant::CastorParticipant(std::shared_ptr<StudyPuller> sp, std::shared_ptr<Participant> participant)
  : mSp(sp), mParticipant(participant) {
  mRepeatingDataInstances = CreateRxCache([sp, participant]() {return sp->getRepeatingDataInstances(participant); });
}

}
}
