#pragma once

#include <pep/rsk/Proofs.hpp>

namespace pep {

struct ServerVerifiers {
  ReshuffleRekeyVerifiers
    accessManager,
    storageFacility,
    transcryptor;

  void ensureThreadSafe() const {
    accessManager.ensureThreadSafe();
    storageFacility.ensureThreadSafe();
    transcryptor.ensureThreadSafe();
  }
};

}
