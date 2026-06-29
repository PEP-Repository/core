#pragma once

#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/auth/Signed.hpp>

namespace pep {

struct PEPIdRegistrationRequest {};

struct PEPIdRegistrationResponse {
  std::string pepId;
};


struct RegistrationRequest {
  PolymorphicPseudonym polymorphicPseudonym;
  std::string encryptedIdentifier;
  std::string encryptionPublicKeyPem;
};

struct RegistrationResponse {};


struct ListCastorImportColumnsRequest {
  std::string spColumn;
  unsigned answerSetCount{};
};

struct ListCastorImportColumnsResponse {
  std::vector<std::string> importColumns;
};


using SignedPEPIdRegistrationRequest = Signed<PEPIdRegistrationRequest>;
using SignedRegistrationRequest = Signed<RegistrationRequest>;

}
