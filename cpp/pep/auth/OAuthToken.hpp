#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>

namespace pep {

class OAuthToken {
private:
  std::string serialized_;

  std::string data_;
  std::string hmac_;

  std::string subject_;
  std::string group_;

  std::chrono::sys_seconds issuedAt_, expiresAt_;

private:
  explicit OAuthToken(const std::string& serialized);

  bool verifyValidityPeriod() const;
  bool verifySubject(const std::string& required) const;
  bool verifyGroup(const std::string& required) const;

public:
  static const std::string DEFAULT_JSON_FILE_NAME;

  OAuthToken() = default;
  const std::string& getSerializedForm() const noexcept { return serialized_; }
  const std::string& getSubject() const noexcept { return subject_; }
  const std::string& getGroup() const noexcept { return group_; }
  std::chrono::sys_seconds getIssuedAt() const noexcept { return issuedAt_; }
  std::chrono::sys_seconds getExpiresAt() const noexcept { return expiresAt_; }

  bool verify(const std::string& secret, const std::string& requiredSubject, const std::string& requiredGroup) const;
  bool verify(const std::optional<std::string>& requiredSubject, const std::optional<std::string>& requiredGroup) const;

  static inline OAuthToken Parse(const std::string& serialized) { return OAuthToken(serialized); }
  static OAuthToken Generate(const std::string& secret, const std::string& subject, const std::string& group,
    std::chrono::sys_seconds issuedAt, std::chrono::sys_seconds expirationTime);

  static OAuthToken ReadJson(std::istream& source);
  static OAuthToken ReadJson(const std::filesystem::path& file);
  void writeJson(std::ostream& destination, bool pretty = true) const;
  void writeJson(const std::filesystem::path& file, bool pretty = true) const;
};

}
