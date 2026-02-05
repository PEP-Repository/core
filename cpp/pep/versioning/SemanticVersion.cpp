#include <pep/versioning/SemanticVersion.hpp>

#include <vector>

#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>
#include <pep/serialization/Error.hpp>

namespace pep {
SemanticVersion::SemanticVersion(
  unsigned int majorVersion,
  unsigned int minorVersion,
  unsigned int build,
  unsigned int revision) : mMajorVersion(majorVersion), mMinorVersion(minorVersion), mBuild(build), mRevision(revision){}

std::string SemanticVersion::format() const {
  std::vector<std::string> semverParts{
    std::to_string(mMajorVersion), 
    std::to_string(mMinorVersion), 
    std::to_string(mBuild),
    std::to_string(mRevision)
  };
  return boost::algorithm::join(semverParts, ".");
}

bool SemanticVersion::hasGitlabProperties() const noexcept {
  return mBuild > 0U && mRevision > 0U;
}

bool IsSemanticVersionEquivalent(const SemanticVersion& lhs, const SemanticVersion& rhs){
  return std::make_tuple<unsigned int, unsigned int, unsigned int>(lhs.getMajorVersion(), lhs.getMinorVersion(), lhs.getBuild()) ==
  std::make_tuple<unsigned int, unsigned int, unsigned int>(rhs.getMajorVersion(), rhs.getMinorVersion(), rhs.getBuild());
}

}
