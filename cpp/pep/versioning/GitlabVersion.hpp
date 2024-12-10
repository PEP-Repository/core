#pragma once

#include <optional>
#include <string>

namespace pep {

class GitlabVersion {
public:
  inline const std::string& getProjectPath() const noexcept { return mProjectPath; } // 'pep/foss', ... (Gitlab project path)
  inline const std::string& getReference() const noexcept { return mReference; } // 'master', ... (git branches or tags)
  inline const std::string& getPipelineId() const noexcept { return mPipelineId; } // '123', ... (GitLab CI pipeline id)
  inline const std::string& getJobId() const noexcept { return mJobId; } // '123', ... (GitLab CI job id)
  inline const std::string& getRevision() const noexcept { return mRevision; } // git revision if available

  GitlabVersion(
    std::string projectPath,
    std::string reference,
    std::string pipelineId,
    std::string jobId,
    std::string revision);
  GitlabVersion(const GitlabVersion &other) = default;
  GitlabVersion(GitlabVersion &&other) = default;
  GitlabVersion& operator=(const GitlabVersion &other) = default;
  GitlabVersion& operator=( GitlabVersion &&other) = default;
  virtual ~GitlabVersion() = default;

  bool isGitlabBuild() const;
  virtual std::string getSummary() const;

protected:
  std::string constructSummary(const std::optional<std::string>& project, bool includeReference) const;
  static std::string ConcatSummaryParts(const std::string& first, const std::string& delim, const std::string& last);

private:
  std::string mProjectPath;
  std::string mReference;
  std::string mPipelineId;
  std::string mJobId;
  std::string mRevision;
};

}
