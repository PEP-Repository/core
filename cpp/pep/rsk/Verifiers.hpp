#pragma once

#include <pep/crypto/X509Certificate.hpp>
#include <pep/rsk/Proofs.hpp>

namespace pep {

class VerifiersRequest {
};

class VerifiersResponse {
public:
  VerifiersResponse() = default;
  VerifiersResponse(
    const ReshuffleRekeyVerifiers& am,
    const ReshuffleRekeyVerifiers& sf,
    const ReshuffleRekeyVerifiers& ts)
    : mAccessManager(am),
    mStorageFacility(sf),
    mTranscryptor(ts) { }

  ReshuffleRekeyVerifiers mAccessManager;
  ReshuffleRekeyVerifiers mStorageFacility;
  ReshuffleRekeyVerifiers mTranscryptor;

  void ensureThreadSafe() const; // See CurvePoint::ensureThreadSafe()
};

struct UserVerifiersRequest {
  X509Certificate userCertificate;
};

struct UserVerifiersResponse {
  ReshuffleRekeyVerifiers verifiers;
  ReshuffleRekeyVerifiersProof proof;
};

}
