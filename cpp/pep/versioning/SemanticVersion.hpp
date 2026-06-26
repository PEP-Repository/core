#pragma once

#include <string>

namespace pep {
class SemanticVersion {
   unsigned int majorVersion_;
   unsigned int minorVersion_;
   unsigned int build_;
   unsigned int revision_;

public:
  inline unsigned int getMajorVersion() const noexcept { return majorVersion_; }
  inline unsigned int getMinorVersion() const noexcept { return minorVersion_; }
  inline unsigned int getBuild() const noexcept { return build_; }
  inline unsigned int getRevision() const noexcept { return revision_; }

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
