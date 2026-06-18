#include <pep/transcryptor/TranscryptorMessages.hpp>

namespace pep {

void TranscryptorRequestEntry::ensurePacked() const {
  mPolymorphic.ensurePacked();
  mAccessManager.ensurePacked();
  mStorageFacility.ensurePacked();
  mTranscryptor.ensurePacked();
  mAccessManagerProof.ensurePacked();
  mStorageFacilityProof.ensurePacked();
  mTranscryptorProof.ensurePacked();
  if (userGroup_)
    userGroup_->ensurePacked();
  if (mUserGroupProof)
    mUserGroupProof->ensurePacked();
}

}
