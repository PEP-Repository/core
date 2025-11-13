#include <pep/versioning/GitlabVersion.hpp>

#include <boost/algorithm/string/join.hpp>

#include <sstream>
#include <string_view>
#include <vector>

namespace {

// Default values for these should be set by CMake.
#ifndef BUILD_PIPELINE_ID
#  error Define BUILD_PIPELINE_ID.
#endif
#ifndef BUILD_JOB_ID
#  error Define BUILD_JOB_ID.
#endif

#ifndef BUILD_REV
#  error Define BUILD_REV.
#endif
#ifndef BUILD_REF
#  error Define BUILD_REF.
#endif

}

// Not checking mProjectPath because legacy servers will not report this field, making us think that they're development versions

namespace pep {

GitlabVersion::GitlabVersion(
    std::string projectPath,
    std::string reference,
    std::string revision,
    unsigned int majorVersion,
    unsigned int minorVersion,
    unsigned int pipelineId,
    unsigned int jobId
    )
  : mProjectPath(std::move(projectPath)),
    mReference(std::move(reference)), mRevision(std::move(revision)),
    mSemver(majorVersion, minorVersion, pipelineId, jobId) {}

bool GitlabVersion::isGitlabBuild() const {
  return mSemver.hasGitlabProperties()
    // Not checking mProjectPath because legacy servers will not report this field, making us think that they're development versions
    && !mRevision.empty()
    && !mReference.empty();
}

std::string GitlabVersion::getSummary() const {
  return this->constructSummary(std::nullopt, true);
}

std::string GitlabVersion::prettyPrint() const {
  std::ostringstream result{};
  result << "Version: " << mSemver.format() << '\n'
         << "Revision: " << getRevision() << '\n'
         << "Project path: " << getProjectPath() << '\n';
  return std::move(result).str();
}

std::string GitlabVersion::constructSummary(const std::optional<std::string>& project, bool includeReference) const {
  auto projectSpec = project.value_or(mProjectPath);
  auto env = includeReference ? ConcatSummaryParts(projectSpec, ":", mReference) : projectSpec;

  return ConcatSummaryParts(env, " ", mSemver.format());
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
} // namespace pep
