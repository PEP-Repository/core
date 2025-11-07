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

  std::string mCryptoNonce;
  std::string mCryptoMac;
  std::string mPayloadData;
  uint64_t mPageNumber = 0;
  uint32_t mIndex = 0;

  // Fills the page with the provided plaintext.
  // Note that mPageNumber should already be set to the right value.,
  void setEncrypted(
          std::string_view plaintext,
          const std::string& key,
          const Metadata& metadata);

  // Decrypts page.
  std::string decrypt(const std::string& key, const Metadata& metadata) const;

  static bool EncryptionIncludesMetadata(EncryptionScheme encryptionScheme);
};

class DataPayloadPageStreamOrder {
  decltype(DataPayloadPage::mIndex) mLatestFileIndex = 0;
  decltype(DataPayloadPage::mPageNumber) mNextPageNumber = 0;

public:
  DataPayloadPageStreamOrder() = default;
  DataPayloadPageStreamOrder(const DataPayloadPageStreamOrder&) = delete;
  DataPayloadPageStreamOrder& operator=(const DataPayloadPageStreamOrder&) = delete;
  DataPayloadPageStreamOrder(DataPayloadPageStreamOrder&&) = default;
  DataPayloadPageStreamOrder& operator=(DataPayloadPageStreamOrder&&) = default;

  auto latestFileIndex() const noexcept { return mLatestFileIndex; }
  auto nextPageNumber() const noexcept { return mNextPageNumber; }

  /// Checks that \c DataPayloadPage::mIndex is increasing compared to the previous page and
  /// that \c DataPayloadPage::mPageNumber increments from 0 with 1 each time.
  /// \throws std::runtime_error If this is not the case
  void check(const DataPayloadPage& page);
};

}
