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
  unsigned int revision) : majorVersion_(majorVersion), minorVersion_(minorVersion), build_(build), revision_(revision){}

std::string SemanticVersion::format() const {
  std::vector<std::string> semverParts{
    std::to_string(majorVersion_), 
    std::to_string(minorVersion_), 
    std::to_string(build_),
    std::to_string(revision_)
  };
  return boost::algorithm::join(semverParts, ".");
}

bool SemanticVersion::hasGitlabProperties() const noexcept {
  return build_ > 0U && revision_ > 0U;
}

bool IsSemanticVersionEquivalent(const SemanticVersion& lhs, const SemanticVersion& rhs){
  return std::make_tuple<unsigned int, unsigned int, unsigned int>(lhs.getMajorVersion(), lhs.getMinorVersion(), lhs.getBuild()) ==
  std::make_tuple<unsigned int, unsigned int, unsigned int>(rhs.getMajorVersion(), rhs.getMinorVersion(), rhs.getBuild());
}

}
