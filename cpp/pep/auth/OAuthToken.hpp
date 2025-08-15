#pragma once

#include <ctime>
#include <optional>
#include <string>

#include <filesystem>

namespace pep {

class OAuthToken {
private:
  std::string mSerialized;

  std::string mData;
  std::string mHmac;

  std::string mSubject;
  std::string mGroup;

  // Initialize time_t fields to C's sentinel value for failures: see https://stackoverflow.com/a/42584556
  std::time_t mIssuedAt{-1};
  std::time_t mExpiresAt{-1};

private:
  explicit OAuthToken(const std::string& serialized);

  bool verifyValidityPeriod() const;
  bool verifySubject(const std::string& required) const;
  bool verifyGroup(const std::string& required) const;

public:
  static const std::string DEFAULT_JSON_FILE_NAME;

  OAuthToken() = default;
  const std::string& getSerializedForm() const noexcept { return mSerialized; }
  const std::string& getSubject() const noexcept { return mSubject; }
  const std::string& getGroup() const noexcept { return mGroup; }
  std::time_t getIssuedAt() const noexcept { return mIssuedAt; }
  std::time_t getExpiresAt() const noexcept { return mExpiresAt; }

  bool verify(const std::string& secret, const std::string& requiredSubject, const std::string& requiredGroup) const;
  bool verify(const std::optional<std::string>& requiredSubject, const std::optional<std::string>& requiredGroup) const;

  static inline OAuthToken Parse(const std::string& serialized) { return OAuthToken(serialized); }
  static OAuthToken Generate(const std::string& secret, const std::string& subject, const std::string& group, time_t issuedAt, time_t expirationTime);

  static OAuthToken ReadJson(std::istream& source);
  static OAuthToken ReadJson(const std::filesystem::path& file);
  void writeJson(std::ostream& destination, bool pretty = true) const;
  void writeJson(const std::filesystem::path& file, bool pretty = true) const;
};

}
