#include <pep/versioning/SemanticVersion.hpp>

#include <vector>

#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>
#include <pep/serialization/Error.hpp>

namespace pep {
SemanticVersion::SemanticVersion(
  unsigned int majorVersion,
  unsigned int minorVersion,
  unsigned int pipelineID,
  unsigned int jobID) : mMajorVersion(majorVersion), mMinorVersion(minorVersion), mPipelineId(pipelineID), mJobId(jobID){}

std::string SemanticVersion::format() const {
  std::vector<std::string> semverParts{
    std::to_string(mMajorVersion), 
    std::to_string(mMinorVersion), 
    std::to_string(mPipelineId),
    std::to_string(mJobId)
  };
  return boost::algorithm::join(semverParts, ".");
}

bool SemanticVersion::hasGitlabProperties() const noexcept {
  return mPipelineId > 0U && mJobId > 0U;
}

bool IsSemanticVersionEquivalent(const SemanticVersion& lhs, const SemanticVersion& rhs){
  return std::make_tuple<unsigned int, unsigned int, unsigned int>(lhs.getMajorVersion(), lhs.getMinorVersion(), lhs.getPipelineId()) ==
  std::make_tuple<unsigned int, unsigned int, unsigned int>(rhs.getMajorVersion(), rhs.getMinorVersion(), rhs.getPipelineId());
}

}
