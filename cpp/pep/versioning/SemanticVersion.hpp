#pragma once

#include <string>

namespace pep {
class SemanticVersion {
   unsigned int mMajorVersion;
   unsigned int mMinorVersion;
   unsigned int mPipelineId;
   unsigned int mJobId;

public:
  inline unsigned int getMajorVersion() const noexcept { return mMajorVersion; }
  inline unsigned int getMinorVersion() const noexcept { return mMinorVersion; }
  inline unsigned int getPipelineId() const noexcept { return mPipelineId; }
  inline unsigned int getJobId() const noexcept { return mJobId; }

  SemanticVersion(
   unsigned int majorVersion,
   unsigned int minorVersion,
   unsigned int pipelineID,
   unsigned int jobID);

  std::string format() const;
  std::strong_ordering operator<=> (const SemanticVersion& other) const = default;
};

/* \brief Checks for equality of only the first three values (major, minor, pipeline).
 * The fourth, JobId, is only used to pinpoint the specific GitLab job that built this particular instance and therefore does not provide any
 * information about the "newness' of the software.
 */
bool IsSemanticVersionEquivalent(const SemanticVersion& lhs, const SemanticVersion& rhs);
}
