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
    unsigned int pipelineId,
    unsigned int jobId,
    std::string target,
    std::string protocolChecksum);

  inline const std::string& getTarget() const noexcept { return mTarget; } // 'mac', 'win', 'linux'
  inline const std::string& getProtocolChecksum() const noexcept { return mProtocolChecksum; } // hex-encoded binary checksum for the network protocol (version)

  std::string getSummary() const override;
  std::string prettyPrint() const override;

  static const BinaryVersion current;

private:
  std::string mTarget;
  std::string mProtocolChecksum;
};

class ConfigVersion : public GitlabVersion {
public:
  ConfigVersion(
    std::string projectPath,
    std::string reference,
    std::string commit,
    unsigned int majorVersion,
    unsigned int minorVersion,
    unsigned int pipelineId,
    unsigned int jobId,
    std::string projectCaption);
  static std::optional<ConfigVersion> TryRead(const std::filesystem::path& directory);

  inline const std::string& getProjectCaption() const noexcept { return mProjectCaption; } // 'dtap', 'ppp' ... (specified in file)
  bool exposesProductionData() const noexcept;

  std::string getSummary() const override;
  std::string prettyPrint() const override;

  static std::optional<ConfigVersion> Current();
  static std::optional<ConfigVersion> TryLoad(const std::filesystem::path& directory);

private:
  static std::optional<ConfigVersion> loaded_;
  static std::optional<std::filesystem::path> loadDir_;

  std::string mProjectCaption;
};

}
