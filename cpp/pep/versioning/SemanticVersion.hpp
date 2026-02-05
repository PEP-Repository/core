#pragma once

#include <string>

namespace pep {
class SemanticVersion {
   unsigned int mMajorVersion;
   unsigned int mMinorVersion;
   unsigned int mBuild;
   unsigned int mRevision;

public:
  inline unsigned int getMajorVersion() const noexcept { return mMajorVersion; }
  inline unsigned int getMinorVersion() const noexcept { return mMinorVersion; }
  inline unsigned int getBuild() const noexcept { return mBuild; }
  inline unsigned int getRevision() const noexcept { return mRevision; }

  SemanticVersion(
   unsigned int majorVersion,
   unsigned int minorVersion,
   unsigned int build,
   unsigned int revision);

  std::string format() const;
  std::strong_ordering operator<=> (const SemanticVersion& other) const = default;
  bool hasGitlabProperties() const noexcept;
};

/* \brief Checks for equality of only the first three values (major, minor, build).
 * The fourth, revision, is only used to pinpoint the specific GitLab job that built this particular instance and therefore does not provide any
 * information about the "newness' of the software.
 */
bool IsSemanticVersionEquivalent(const SemanticVersion& lhs, const SemanticVersion& rhs);
}
