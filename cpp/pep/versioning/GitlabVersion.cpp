#include <pep/versioning/GitlabVersion.hpp>

#include <boost/algorithm/string/join.hpp>

#include <sstream>
#include <string_view>
#include <vector>

namespace pep {

GitlabVersion::GitlabVersion(
    std::string projectPath,
    std::string reference,
    std::string commit,
    unsigned int majorVersion,
    unsigned int minorVersion,
    unsigned int build,
    unsigned int revision
    )
  : mProjectPath(std::move(projectPath)),
    mReference(std::move(reference)), mCommit(std::move(commit)),
    mSemver(majorVersion, minorVersion, build, revision) {}

bool GitlabVersion::isGitlabBuild() const {
  return mSemver.hasGitlabProperties()
    // Not checking mProjectPath because legacy servers will not report this field, making us think that they're development versions
    && !mCommit.empty()
    && !mReference.empty();
}

std::string GitlabVersion::getSummary() const {
  return this->constructSummary(std::nullopt, true);
}

std::string GitlabVersion::prettyPrint() const {
  std::ostringstream result{};
  result << "Version: " << mSemver.format() << '\n'
         << "Commit: " << getCommit() << '\n'
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
