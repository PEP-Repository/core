#pragma once

#include <pep/versioning/GitlabVersion.hpp>

#include <filesystem>

namespace pep {

class BinaryVersion : public GitlabVersion {
public:
  BinaryVersion(
    std::string projectPath,
    std::string reference,
    std::string commit,
    unsigned int majorVersion,
    unsigned int minorVersion,
    unsigned int build,
    unsigned int revision,
    std::string target,
    std::string protocolChecksum);

  inline const std::string& getTarget() const noexcept { return target_; } // 'mac', 'win', 'linux'
  inline const std::string& getProtocolChecksum() const noexcept { return protocolChecksum_; } // hex-encoded binary checksum for the network protocol (version)

  std::string getSummary() const override;
  std::string prettyPrint() const override;

  static const BinaryVersion current;

private:
  std::string target_;
  std::string protocolChecksum_;
};

class ConfigVersion : public GitlabVersion {
public:
  ConfigVersion(
    std::string projectPath,
    std::string reference,
    std::string commit,
    unsigned int majorVersion,
    unsigned int minorVersion,
    unsigned int build,
    unsigned int revision,
    std::string projectCaption);
  static std::optional<ConfigVersion> TryRead(const std::filesystem::path& directory);

  inline const std::string& getProjectCaption() const noexcept { return projectCaption_; } // 'dtap', 'ppp' ... (specified in file)
  bool exposesProductionData() const noexcept;

  std::string getSummary() const override;
  std::string prettyPrint() const override;

  static std::optional<ConfigVersion> Current();
  static std::optional<ConfigVersion> TryLoad(const std::filesystem::path& directory);

private:
  static std::optional<ConfigVersion> loaded_;
  static std::optional<std::filesystem::path> loadDir_;

  std::string projectCaption_;
};

}
