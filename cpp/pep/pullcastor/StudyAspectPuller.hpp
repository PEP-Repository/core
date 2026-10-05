#pragma once

#include <pep/pullcastor/CastorParticipant.hpp>
#include <pep/utils/EnumUtils.hpp>

namespace pep {
namespace castor {

/// \brief (Base class for classes that) pulls Castor data for a specific study aspect, e.g. "SURVEYs for study XYZ".
class StudyAspectPuller : public std::enable_shared_from_this<StudyAspectPuller>, boost::noncopyable {

private:
  std::shared_ptr<StudyPuller> study_;
  std::string spColumn_;
  std::string columnNamePrefix_;

protected:
  StudyAspectPuller(std::shared_ptr<StudyPuller> study, const StudyAspect& aspect);

  /// \brief Produces the prefix to use for column names when importing data for this study aspect.
  /// \return The prefix for column names.
  inline const std::string& getColumnNamePrefix() const noexcept { return columnNamePrefix_; }

public:
  virtual ~StudyAspectPuller() = default;

  /// \brief Produces (an observable emitting) all StudyAspectPuller instances corresponding with the specified study.
  /// \param study The StudyPuller instance for a particular Castor study.
  /// \return StudyAspectPuller instances for all aspects that should be pulled from the specified study.
  static rxcpp::observable<std::shared_ptr<StudyAspectPuller>> CreateChildrenFor(std::shared_ptr<StudyPuller> study);

  /// \brief Produces (an observable emitting) the Castor content to store for the specified participant.
  /// \param participant The CastorParticipant instance representing the participant to process.
  /// \return Entries representing data that should be stored in PEP for the specified Castor participant.
  virtual rxcpp::observable<std::shared_ptr<StorableColumnContent>> getStorableContent(std::shared_ptr<CastorParticipant> participant) = 0;

  /// \brief Produces The StudyPuller instance associated with this StudyAspectPuller.
  /// \return A StudyPuller instance.
  inline std::shared_ptr<StudyPuller> getStudyPuller() const noexcept { return study_; }

  /// \brief Produces The short pseudonym column name associated with this StudyAspectPuller.
  /// \return The name of a PEP column that stores short pseudonym values.
  inline const std::string& getShortPseudonymColumn() const noexcept { return spColumn_; }
};

}
}
