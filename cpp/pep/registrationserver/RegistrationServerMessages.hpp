#pragma once

#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/crypto/Signed.hpp>

namespace pep {

class PEPIdRegistrationRequest {
};

class PEPIdRegistrationResponse {
public:
  std::string mPepId;
};


class RegistrationRequest {
public:
  RegistrationRequest() = default;
  explicit RegistrationRequest(const PolymorphicPseudonym& polymorphicPseudonym) : mPolymorphicPseudonym(polymorphicPseudonym) { }
  PolymorphicPseudonym mPolymorphicPseudonym;
  std::string mEncryptedIdentifier;
  std::string mEncryptionPublicKeyPem;
};

class RegistrationResponse {
};


class ListCastorImportColumnsRequest {
public:
  std::string mSpColumn;
  unsigned mAnswerSetCount{};
};

class ListCastorImportColumnsResponse {
public:
  std::vector<std::string> mImportColumns;
};


using SignedPEPIdRegistrationRequest = Signed<PEPIdRegistrationRequest>;
using SignedRegistrationRequest = Signed<RegistrationRequest>;

}
