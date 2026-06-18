#include <pep/transcryptor/TranscryptorMessages.hpp>

namespace pep {

void TranscryptorRequestEntry::ensurePacked() const {
  polymorphic_.ensurePacked();
  accessManager_.ensurePacked();
  storageFacility_.ensurePacked();
  transcryptor_.ensurePacked();
  accessManagerProof_.ensurePacked();
  storageFacilityProof_.ensurePacked();
  transcryptorProof_.ensurePacked();
  if (userGroup_)
    userGroup_->ensurePacked();
  if (userGroupProof_)
    userGroupProof_->ensurePacked();
}

}
