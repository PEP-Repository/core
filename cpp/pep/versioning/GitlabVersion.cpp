#include <pep/versioning/GitlabVersion.hpp>

namespace pep {


GitlabVersion::GitlabVersion(
    std::string projectPath,
    std::string reference,
    std::string pipelineId,
    std::string jobId,
    std::string revision)
  : mProjectPath(std::move(projectPath)),
    mReference(std::move(reference)),
    mPipelineId(std::move(pipelineId)),
    mJobId(std::move(jobId)),
    mRevision(std::move(revision)) {
}

bool GitlabVersion::isGitlabBuild() const {
  // Not checking mProjectPath because legacy servers will not report this field, making us think that they're development versions
  return !(mReference.empty() || mPipelineId.empty() || mJobId.empty() || mRevision.empty());
}

std::string GitlabVersion::getSummary() const {
  return this->constructSummary(std::nullopt, true);
}

std::string GitlabVersion::constructSummary(const std::optional<std::string>& project, bool includeReference) const {
  auto projectSpec = project.value_or(mProjectPath);
  auto env = includeReference ? ConcatSummaryParts(projectSpec, ":", mReference) : projectSpec;
  auto ver = ConcatSummaryParts(mPipelineId, "/", mJobId);
  return ConcatSummaryParts(env, " ", ver);
}

std::string GitlabVersion::ConcatSummaryParts(const std::string& first, const std::string& delim, const std::string& last) {
  std::string result = first;
  if (!last.empty()) {
    if (!result.empty()) {
      result += delim;
    }
    result += last;
  }
  return result;
}

}
