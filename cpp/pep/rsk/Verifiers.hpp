#pragma once

#include <pep/rsk/Proofs.hpp>

namespace pep {

class VerifiersRequest {
};

class VerifiersResponse {
public:
  VerifiersResponse() = default;
  VerifiersResponse(
    RSKVerifiers am,
    RSKVerifiers sf,
    RSKVerifiers ts)
    : mAccessManager(am),
    mStorageFacility(sf),
    mTranscryptor(ts) { }

  RSKVerifiers mAccessManager;
  RSKVerifiers mStorageFacility;
  RSKVerifiers mTranscryptor;

  void ensureThreadSafe() const; // See CurvePoint::ensureThreadSafe()
};

}
