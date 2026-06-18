#include <pep/pullcastor/CastorParticipant.hpp>

namespace pep {
namespace castor {

CastorParticipant::CastorParticipant(std::shared_ptr<StudyPuller> sp, std::shared_ptr<Participant> participant)
  : sp_(sp), participant_(participant) {
  repeatingDataInstances_ = CreateRxCache([sp, participant]() {return sp->getRepeatingDataInstances(participant); });
}

}
}
