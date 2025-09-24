#include <pep/rsk/Verifiers.hpp>

namespace pep {

void VerifiersResponse::ensureThreadSafe() const {
  mAccessManager.ensureThreadSafe();
  mStorageFacility.ensureThreadSafe();
  mTranscryptor.ensureThreadSafe();
}

}
