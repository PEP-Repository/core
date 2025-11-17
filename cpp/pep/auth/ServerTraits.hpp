#pragma once

#include <pep/auth/EnrolledParty.hpp>
#include <pep/crypto/X509Certificate.hpp>
#include <functional>
#include <unordered_set>

namespace pep {

class ServerTraits {
private:
  std::string mAbbreviation;
  std::string mDescription;
  std::optional<EnrolledParty> mEnrollsAs;
  std::optional<std::string> mCustomId;

  std::string defaultId() const;
  std::string id() const;

  ServerTraits(std::string abbreviation, std::string description) noexcept;
  ServerTraits(std::string abbreviation, std::string description, EnrolledParty enrollsAs) noexcept;
  ServerTraits(std::string abbreviation, std::string description, std::string customId) noexcept;

public:
  const std::string& abbreviation() const noexcept { return mAbbreviation; }
  const std::string& description() const noexcept { return mDescription; }

  std::string tlsCertificateSubject() const;
  std::string configNode() const;
  std::string commandLineId() const;

  bool hasSigningIdentity() const;
  std::optional<std::string> userGroup() const;
  std::unordered_set<std::string> userGroups() const;
  bool signingIdentityMatches(const std::string& certificateSubject) const;
  bool signingIdentityMatches(const X509Certificate& certificate) const;
  bool signingIdentityMatches(const X509CertificateChain& chain) const;
  
  bool isEnrollable() const;
  const std::optional<EnrolledParty>& enrollsAs() const noexcept { return mEnrollsAs; }
  std::optional<std::string> enrollmentSubject() const;

  bool hasDataAccess() const;

  static ServerTraits AccessManager();
  static ServerTraits AuthServer();
  static ServerTraits KeyServer();
  static ServerTraits RegistrationServer();
  static ServerTraits StorageFacility();
  static ServerTraits Transcryptor();

  static std::vector<ServerTraits> All();
  static std::optional<ServerTraits> Find(const std::function<bool(const ServerTraits&)>& predicate);
  static std::optional<ServerTraits> Find(EnrolledParty enrollsAs);

  auto operator<=>(const ServerTraits&) const = default;
};

}
