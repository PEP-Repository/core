#pragma once

#include <pep/pullcastor/CastorParticipant.hpp>
#include <pep/utils/SelfRegistering.hpp>

namespace pep {
namespace castor {

/*!
  * \brief (Base class for classes that) pulls Castor data for a specific study aspect, e.g. "SURVEYs for study XYZ".
  */
class StudyAspectPuller : public std::enable_shared_from_this<StudyAspectPuller>, boost::noncopyable {
  template <typename TDerived, typename TRegistrar, bool>
  friend class pep::SelfRegistering;

private:
  std::shared_ptr<StudyPuller> mStudy;
  std::string mSpColumn;
  std::string mColumnNamePrefix;

  // A function that can create a (shared_ptr to) StudyAspectPuller. Such functions are statically registered by derived TypedStudyAspectPuller<> class.
  using CreateFunction = std::function<std::shared_ptr<StudyAspectPuller>(std::shared_ptr<StudyPuller>, const StudyAspect&)>;

  // Prevent trouble w.r.t. static initialization order by wrapping our (static) list of CreateFunction in a function.
  static std::unordered_map<CastorStudyType, CreateFunction>& GetCreateFunctions();

protected:
  StudyAspectPuller(std::shared_ptr<StudyPuller> study, const StudyAspect& aspect);

  /*!
  * \brief Registers the specified TDerived type as the handler for the import of its specified aspect STUDY_TYPE.
  * \tparam TDerived The derived type that imports the specified STUDY_TYPE.
  * \return The STUDY_TYPE specified by TDerived.
  */
  template <typename TDerived>
  static CastorStudyType RegisterType() {
    static_assert(std::is_base_of<StudyAspectPuller, TDerived>::value);

    // Raw &TDerived::Create is (too) difficult to cast to a CreateFunction, so we wrap it in a lambda
    CreateFunction createDerived = [](std::shared_ptr<StudyPuller> study, const StudyAspect& aspect) {
      return TDerived::Create(study, aspect);
    };

    auto& registered = GetCreateFunctions();
    if (!registered.emplace(std::make_pair(TDerived::STUDY_TYPE, createDerived)).second) {
      throw std::runtime_error("Duplicate registration for study aspect puller type for Castor study type " + std::to_string(TDerived::STUDY_TYPE));
    }

    return TDerived::STUDY_TYPE;
  }

  /*!
  * \brief Produces the prefix to use for column names when importing data for this study aspect.
  * \return The prefix for column names.
  */
  inline const std::string& getColumnNamePrefix() const noexcept { return mColumnNamePrefix; }

public:
  virtual ~StudyAspectPuller() = default;

  /*!
  * \brief Produces (an observable emitting) all StudyAspectPuller instances corresponding with the specified study.
  * \param study The StudyPuller instance for a particular Castor study.
  * \return StudyAspectPuller instances for all aspects that should be pulled from the specified study.
  */
  static rxcpp::observable<std::shared_ptr<StudyAspectPuller>> CreateChildrenFor(std::shared_ptr<StudyPuller> study);

  /*!
  * \brief Produces (an observable emitting) the Castor content to store for the specified participant.
  * \param participant The CastorParticipant instance representing the participant to process.
  * \return Entries representing data that should be stored in PEP for the specified Castor participant.
  */
  virtual rxcpp::observable<std::shared_ptr<StorableColumnContent>> getStorableContent(std::shared_ptr<CastorParticipant> participant) = 0;

  /*!
  * \brief Produces The StudyPuller instance associated with this StudyAspectPuller.
  * \return A StudyPuller instance.
  */
  inline std::shared_ptr<StudyPuller> getStudyPuller() const noexcept { return mStudy; }

  /*!
  * \brief Produces The short pseudonym column name associated with this StudyAspectPuller.
  * \return The name of a PEP column that stores short pseudonym values.
  */
  inline const std::string& getShortPseudonymColumn() const noexcept { return mSpColumn; }
};

/*!
  * \brief Helper (base) class that automatically registers the TDerived aspect puller type as the handler for the specified study aspect TYPE.
  */
template <typename TDerived, CastorStudyType TYPE, bool REGISTER = true>
class TypedStudyAspectPuller : public StudyAspectPuller, public SelfRegistering<TDerived, StudyAspectPuller, REGISTER> {
public:
  static constexpr CastorStudyType STUDY_TYPE = TYPE;

protected:
  inline TypedStudyAspectPuller(std::shared_ptr<StudyPuller> study, const StudyAspect& aspect)
    : StudyAspectPuller(study, aspect) {
  }
};

}
}
