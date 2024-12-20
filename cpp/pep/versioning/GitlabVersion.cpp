#include <pep/versioning/GitlabVersion.hpp>

#include <boost/algorithm/string/join.hpp>

#include <sstream>
#include <vector>

namespace {

// Checking for empty preprocessor symbol: https://stackoverflow.com/a/15380877
// Not checking mProjectPath because legacy servers will not report this field, making us think that they're development versions

#if BOOST_PP_IS_EMPTY(BUILD_PIPELINE_ID) || BOOST_PP_IS_EMPTY(BUILD_JOB_ID) || BOOST_PP_IS_EMPTY(BUILD_REV) || BOOST_PP_IS_EMPTY(BUILD_REF)
constexpr bool IS_GITLAB_BUILD = false;
#else
constexpr bool IS_GITLAB_BUILD = true;
#endif

}

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
  return IS_GITLAB_BUILD;
}

std::string GitlabVersion::getSummary() const { 
  return this->constructSummary(std::nullopt, true); 
}

std::string GitlabVersion::prettyPrint() const {
  std::stringstream result{};
  result << "Version: " << mSemver.format() << '\n'
         << "Revision: " << getRevision() << '\n'
         << "Project path: " << getProjectPath() << '\n';
  return result.str();
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
