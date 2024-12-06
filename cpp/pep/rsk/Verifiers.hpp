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
    : mAccessManager(std::move(am)),
    mStorageFacility(std::move(sf)),
    mTranscryptor(std::move(ts)) { }

  RSKVerifiers mAccessManager;
  RSKVerifiers mStorageFacility;
  RSKVerifiers mTranscryptor;

  void ensureThreadSafe() const; // See CurvePoint::ensureThreadSafe()
};

}
