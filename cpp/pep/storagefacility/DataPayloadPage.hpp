#pragma once

#include <exception>

#include <pep/morphing/Metadata.hpp>

namespace pep {

class PageIntegrityError : public std::exception {
  const char* what() const noexcept override {
    return "PageIntegrityError";
  }
};

class DataPayloadPage {
 private:
  std::string computeAdditionalData(const Metadata& metadata) const;

 public:
  DataPayloadPage() = default;

  // TODO Reuse Encrypted<T> instead of duplicating the symmetric crypto

  std::string cryptoNonce_;
  std::string cryptoMac_;
  std::string payloadData_;
  uint64_t pageNumber_ = 0;
  uint32_t index_ = 0;

  // Fills the page with the provided plaintext.
  // Note that pageNumber_ should already be set to the right value.,
  void setEncrypted(
          std::string_view plaintext,
          const std::string& key,
          const Metadata& metadata);

  // Decrypts page.
  std::string decrypt(const std::string& key, const Metadata& metadata) const;

  static bool EncryptionIncludesMetadata(EncryptionScheme encryptionScheme);
};

}
