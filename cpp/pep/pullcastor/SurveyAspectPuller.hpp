#pragma once

#include <pep/castor/SurveyPackageInstance.hpp>
#include <pep/castor/SurveyStep.hpp>
#include <pep/pullcastor/StudyAspectPuller.hpp>
#include <pep/pullcastor/TimestampedSpi.hpp>

namespace pep {
namespace castor {

/*!
  * \brief Pulls Castor survey ("SURVEY") data for a single Castor study.
  */
class SurveyAspectPuller : public TypedStudyAspectPuller<SurveyAspectPuller, CastorStudyType::SURVEY>, private SharedConstructor<SurveyAspectPuller> {
  friend class StudyAspectPuller;
  friend class SharedConstructor<SurveyAspectPuller>;

private:
  class SpisPuller;
  class AllSpisPuller;
  class LatestSpiPuller;

  using Spis = std::vector<std::shared_ptr<SurveyPackageInstance>>;
  using SpisById = std::unordered_map<std::string, std::shared_ptr<Spis>>;
  using Sdps = std::vector<std::shared_ptr<SurveyDataPoint>>;
  using SdpsBySpi = std::unordered_map<std::shared_ptr<SurveyPackageInstance>, std::shared_ptr<Sdps>>;

  std::shared_ptr<RxCache<std::shared_ptr<SurveyPackageInstance>>> mSpis;
  std::shared_ptr<RxCache<std::shared_ptr<SpisById>>> mSpisByParticipantId;
  std::shared_ptr<RxCache<std::shared_ptr<SdpsBySpi>>> mSdpsBySpi;
  std::shared_ptr<SpisPuller> mSpisPuller;

  SurveyAspectPuller(std::shared_ptr<StudyPuller> sp, const StudyAspect& aspect);

  rxcpp::observable<std::shared_ptr<SurveyDataPoint>> getDataPoints(std::shared_ptr<SurveyPackageInstance> spi);
  rxcpp::observable<std::shared_ptr<SdpsBySpi>> getDataPoints(std::shared_ptr<Spis> spis);

public:
  /*!
  * \brief Produces (an observable emitting) the Castor content to store for the specified participant.
  * \param participant The CastorParticipant instance representing the participant to process.
  * \return Entries representing the Castor survey data that should be stored in PEP for the specified Castor participant.
  */
  rxcpp::observable<std::shared_ptr<StorableColumnContent>> getStorableContent(std::shared_ptr<CastorParticipant> participant) override;
};

}
}
