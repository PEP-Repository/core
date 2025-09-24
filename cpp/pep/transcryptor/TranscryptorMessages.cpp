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
  if (mUserGroup)
    mUserGroup->ensurePacked();
  if (mUserGroupProof)
    mUserGroupProof->ensurePacked();
}

}
