#pragma once

#include <optional>
#include <string>

#include <pep/versioning/SemanticVersion.hpp>

namespace pep {

class GitlabVersion {
public:
  SemanticVersion getSemver() const { return mSemver;}
  inline const std::string& getProjectPath() const noexcept { return mProjectPath; } // 'pep/foss', ... (Gitlab project path)
  inline const std::string& getReference() const noexcept { return mReference; } // 'master', ... (git branches or tags)
  inline const std::string& getCommit() const noexcept { return mCommit; } // git commit (SHA) if available

  GitlabVersion(
    std::string projectPath,
    std::string reference,
    std::string commit,
    unsigned int majorVersion,
    unsigned int minorVersion,
    unsigned int pipelineId,
    unsigned int jobId);

  GitlabVersion(const GitlabVersion& other) = default;
  GitlabVersion(GitlabVersion&& other) = default;
  GitlabVersion& operator=(const GitlabVersion& other) = default;
  GitlabVersion& operator=(GitlabVersion&& other) = default;
  virtual ~GitlabVersion() = default;

  bool isGitlabBuild() const;
  virtual std::string getSummary() const;
  virtual std::string prettyPrint() const;

protected:
  std::string constructSummary(const std::optional<std::string>& project, bool includeReference) const;
  static std::string ConcatSummaryParts(const std::string& first, const std::string& delim, const std::string& last);

private:
  std::string mProjectPath;
  std::string mReference;
  std::string mCommit;
  SemanticVersion mSemver;
};
} // namespace pep
