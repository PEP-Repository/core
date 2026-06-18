#pragma once

#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/auth/Signed.hpp>

namespace pep {

class PEPIdRegistrationRequest {
};

class PEPIdRegistrationResponse {
public:
  std::string pepId_;
};


class RegistrationRequest {
public:
  RegistrationRequest() = default;
  explicit RegistrationRequest(const PolymorphicPseudonym& polymorphicPseudonym) : polymorphicPseudonym_(polymorphicPseudonym) { }
  PolymorphicPseudonym polymorphicPseudonym_;
  std::string encryptedIdentifier_;
  std::string encryptionPublicKeyPem_;
};

class RegistrationResponse {
};


class ListCastorImportColumnsRequest {
public:
  std::string spColumn_;
  unsigned answerSetCount_{};
};

class ListCastorImportColumnsResponse {
public:
  std::vector<std::string> importColumns_;
};


using SignedPEPIdRegistrationRequest = Signed<PEPIdRegistrationRequest>;
using SignedRegistrationRequest = Signed<RegistrationRequest>;

}
