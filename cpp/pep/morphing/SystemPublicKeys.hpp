#pragma once

#include <pep/elgamal/ElgamalEncryption.hpp>

namespace pep {

struct SystemPublicKeys {
  ElgamalPublicKey globalPseudonymEncryptionKey;
  ElgamalPublicKey globalDataEncryptionKey;
};

}
