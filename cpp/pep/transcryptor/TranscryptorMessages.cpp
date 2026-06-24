#include <pep/transcryptor/TranscryptorMessages.hpp>

namespace pep {

void TranscryptorRequestEntry::ensurePacked() const {
  polymorphic.ensurePacked();
  accessManager.ensurePacked();
  storageFacility.ensurePacked();
  transcryptor.ensurePacked();
  accessManagerProof.ensurePacked();
  storageFacilityProof.ensurePacked();
  transcryptorProof.ensurePacked();
  if (userGroup)
    userGroup->ensurePacked();
  if (userGroupProof)
    userGroupProof->ensurePacked();
}

}
