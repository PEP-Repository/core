#include <pep/content/ParticipantDeviceHistory.hpp>
#include <pep/castor/SurveyDataPoint.hpp>
#include <pep/pullcastor/SurveyPackageInstancePuller.hpp>
#include <pep/pullcastor/SurveyAspectPuller.hpp>
#include <cmath>

namespace pep {
namespace castor {

namespace {

int GetWeekNumber(Timestamp moment, Timestamp offset) {
  auto seconds = difftime(moment.toTime_t(), offset.toTime_t());
  if (seconds < 0) {
    PULLCASTOR_LOG(warning) << "Returning negative week number for timestamp that's before the offset";
  }
  auto weeks = seconds
    / 60 // minutes
    / 60 // hours
    / 24 // days
    / 7; // weeks
  // Explicit floor to handle negative numbers, which may occur,
  //  see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/1654
  return static_cast<int>(std::floor(weeks));
}

using StudyStartTimestamp = decltype(ParticipantDeviceRecord::time);

rxcpp::observable<StudyStartTimestamp> GetStudyStart(const PepParticipant& participant, const std::string& dhCol) {
  auto content = participant.tryGetCellContent(dhCol);
  if (content == nullptr) {
    return rxcpp::observable<>::empty<StudyStartTimestamp>();
  }
  return content->getData()
    .flat_map([dhCol](const std::string& data) -> rxcpp::observable<StudyStartTimestamp> {
    auto history = ParticipantDeviceHistory::Parse(data);
    auto first = history.begin();
    if (first == history.end()) {
      return rxcpp::observable<>::empty<StudyStartTimestamp>();
    }
    return rxcpp::observable<>::just(first->time);
    });
}

using Sdps = std::vector<std::shared_ptr<SurveyDataPoint>>;
using SdpsBySpi = std::unordered_map<std::shared_ptr<SurveyPackageInstance>, std::shared_ptr<Sdps>>;
using SdpsBySpiCache = RxCache<std::shared_ptr<SdpsBySpi>>;

}

class SurveyAspectPuller::SpisPuller : public std::enable_shared_from_this<SpisPuller>, boost::noncopyable {
protected:
  using SurveyStepsById = std::unordered_map<std::string, std::shared_ptr<SurveyStep>>;

private:
  std::shared_ptr<StudyPuller> mSp;
  std::string mColumnNamePrefix;
  std::shared_ptr<RxCache<std::shared_ptr<SurveyStepsById>>> mSurveyStepsById;

protected:
  SpisPuller(std::shared_ptr<StudyPuller> sp, const std::string& columnNamePrefix);

  inline std::shared_ptr<StudyPuller> getStudyPuller() const noexcept { return mSp; }
  inline const std::string& getColumnNamePrefix() const noexcept { return mColumnNamePrefix; }

  rxcpp::observable<std::shared_ptr<StorableColumnContent>> loadContentForSpi(std::shared_ptr<SurveyPackageInstancePuller> spiPuller, rxcpp::observable<std::shared_ptr<SurveyDataPoint>> sdps);

public:
  virtual ~SpisPuller() = default;

  virtual rxcpp::observable<std::shared_ptr<StorableColumnContent>> loadContentForSpis(std::shared_ptr<Spis> spis, std::shared_ptr<SurveyAspectPuller> sp) = 0;
};


class SurveyAspectPuller::AllSpisPuller : public SurveyAspectPuller::SpisPuller, public SharedConstructor<AllSpisPuller> {
  friend class SharedConstructor<AllSpisPuller>;

private:
  using StudyStartsByParticipantId = std::unordered_map<std::string, Timestamp>;

  std::string mDeviceHistoryColumn;
  std::shared_ptr<RxCache<std::shared_ptr<StudyStartsByParticipantId>>> mStudyStartsByParticipantId;

  AllSpisPuller(std::shared_ptr<StudyPuller> sp, const std::string& spColumnName, const std::string& columnNamePrefix, const std::string& deviceHistoryColumn);

  rxcpp::observable<Timestamp> getWeekNumberOffsetForParticipant(const std::string& participantId);

public:
  rxcpp::observable<std::shared_ptr<StorableColumnContent>> loadContentForSpis(std::shared_ptr<Spis> spis, std::shared_ptr<SurveyAspectPuller> sp) override;
};



class SurveyAspectPuller::LatestSpiPuller : public SurveyAspectPuller::SpisPuller, public SharedConstructor<LatestSpiPuller> {
  friend class SharedConstructor<LatestSpiPuller>;

private:
  inline LatestSpiPuller(std::shared_ptr<StudyPuller> sp, const std::string& columnNamePrefix) noexcept;

public:
  rxcpp::observable<std::shared_ptr<StorableColumnContent>> loadContentForSpis(std::shared_ptr<Spis> spis, std::shared_ptr<SurveyAspectPuller> sp) override;
};


SurveyAspectPuller::LatestSpiPuller::LatestSpiPuller(std::shared_ptr<StudyPuller> sp, const std::string& columnNamePrefix) noexcept
  : SpisPuller(sp, columnNamePrefix) {
}

SurveyAspectPuller::SurveyAspectPuller(std::shared_ptr<StudyPuller> sp, const StudyAspect& aspect)
  : TypedStudyAspectPuller<SurveyAspectPuller, CastorStudyType::SURVEY>(sp, aspect) {

  mSpis = CreateRxCache([sp]() {
    return SurveyPackageInstance::BulkRetrieve(sp->getStudy(), sp->getParticipants());
    });
  mSpisByParticipantId = CreateRxCache([spis = mSpis]() {
    return spis->observe()
      .filter([](std::shared_ptr<SurveyPackageInstance> spi) {
      bool result = !spi->isArchived();
      if (!result) {
        PULLCASTOR_LOG(debug) << "Skipping archived SPI " << spi->getId()
          << " for package '" << spi->getSurveyPackageName() << "'"
          << " for participant " << spi->getParticipantId();
      }
      return result;
      })
      .filter([](std::shared_ptr<SurveyPackageInstance> spi) { return spi->getFinishedOn(); })
      .op(RxGroupToVectors([](std::shared_ptr<SurveyPackageInstance> spi) {return spi->getParticipantId(); }));
    });
  // Bulk-retrieve and cache SDP data if we're processing all participants
  if (!sp->getEnvironmentPuller()->getShortPseudonymsToProcess().has_value()) {
    mSdpsBySpi = CreateRxCache([study = sp->getStudy(), spis = mSpis]() {
      return SurveyDataPoint::BulkRetrieve(study, spis->observe())
        .op(RxGroupToVectors([](std::shared_ptr<SurveyDataPoint> sdp) {return sdp->getSurveyPackageInstance(); }));
      });
  }
  const auto& offsetCol = aspect.getStorage()->getWeekOffsetDeviceColumn();
  if (!offsetCol.empty()) {
    mSpisPuller = AllSpisPuller::Create(sp, aspect.getShortPseudonymColumn(), this->getColumnNamePrefix(), offsetCol);
  }
  else {
    mSpisPuller = LatestSpiPuller::Create(sp, this->getColumnNamePrefix());
  }
}

rxcpp::observable<std::shared_ptr<SurveyDataPoint>> SurveyAspectPuller::getDataPoints(std::shared_ptr<SurveyPackageInstance> spi) {
  if (mSdpsBySpi != nullptr) {
    return mSdpsBySpi->observe()
      .flat_map([spi](std::shared_ptr<SdpsBySpi> sdpsBySpi) -> rxcpp::observable<std::shared_ptr<SurveyDataPoint>> {
      auto position = sdpsBySpi->find(spi);
      if (position == sdpsBySpi->cend()) {
        return rxcpp::observable<>::empty<std::shared_ptr<SurveyDataPoint>>();
      }
      return rxcpp::observable<>::iterate(*position->second);
        });
  }
  return spi->getSurveyDataPoints();
}

rxcpp::observable<std::shared_ptr<SdpsBySpi>> SurveyAspectPuller::getDataPoints(std::shared_ptr<Spis> spis) {
  if (mSdpsBySpi != nullptr) {
    return mSdpsBySpi->observe(); // Also contains SDPs for SPIs that caller didn't ask for, but AllSpisPuller::loadContentForSpis won't process entries for those excess SPIs
  }

  assert(!spis->empty());
  auto participant = spis->front()->getParticipant();
  assert(std::all_of(spis->cbegin(), spis->cend(), [participant](std::shared_ptr<SurveyPackageInstance> spi) {return spi->getParticipant() == participant; }));
  return SurveyDataPoint::BulkRetrieve(participant, rxcpp::observable<>::iterate(*spis))
    .op(RxGroupToVectors([](std::shared_ptr<SurveyDataPoint> sdp) {return sdp->getSurveyPackageInstance(); }));
}

SurveyAspectPuller::SpisPuller::SpisPuller(std::shared_ptr<StudyPuller> sp, const std::string& columnNamePrefix)
  : mSp(sp), mColumnNamePrefix(columnNamePrefix) {
  mSurveyStepsById = CreateRxCache([study = sp->getStudy()]() {
    return study->getSurveys()
      .flat_map([](std::shared_ptr<Survey> survey) {return survey->getSteps(); })
      .op(RxToUnorderedMap([](std::shared_ptr<SurveyStep> step) {return step->getId(); }));
  });
}

SurveyAspectPuller::AllSpisPuller::AllSpisPuller(std::shared_ptr<StudyPuller> sp, const std::string& spColumnName, const std::string& columnNamePrefix, const std::string& deviceHistoryColumn)
  : SpisPuller(sp, columnNamePrefix), mDeviceHistoryColumn(deviceHistoryColumn) {
  mStudyStartsByParticipantId = CreateRxCache([ep = this->getStudyPuller()->getEnvironmentPuller(), spCol = spColumnName, dhCol = deviceHistoryColumn]() {
    return ep->getStoredData()
      .flat_map([spCol, dhCol](std::shared_ptr<StoredData> stored) {
      return stored->getParticipants()
        .flat_map([spCol, dhCol, stored](std::shared_ptr<const PepParticipant> participant) -> rxcpp::observable<std::pair<std::string, StudyStartTimestamp>> {
        return GetStudyStart(*participant, dhCol)
          .flat_map([spCol, stored, participant](const StudyStartTimestamp& started) {
          return stored->getCastorSps(participant, spCol)
            .map([timestamp = started](const std::string& sp) {return std::make_pair(sp, timestamp); });
          });
        })
        .reduce(
          std::make_shared<StudyStartsByParticipantId>(),
          [](std::shared_ptr<StudyStartsByParticipantId> result, const auto& pair) {
            auto added = result->emplace(pair).second;
            if (!added) {
              throw std::runtime_error("Could not add duplicate Castor SP to study starts");
            }
            return result;
          }
        );
      });
    });
}

rxcpp::observable<std::shared_ptr<StorableColumnContent>> SurveyAspectPuller::SpisPuller::loadContentForSpi(std::shared_ptr<SurveyPackageInstancePuller> spiPuller, rxcpp::observable<std::shared_ptr<SurveyDataPoint>> sdps) {
  return sdps
    .op(RxSharedPtrCast<DataPointBase>())
    .flat_map([sp = this->getStudyPuller()](std::shared_ptr<DataPointBase> dp) {return sp->toFieldValue(dp).op(RxGetOne("survey field value")); })
    .group_by([](std::shared_ptr<FieldValue> fv) {return fv->getField()->getParentId(); })
    .flat_map([self = SharedFrom(*this), spiPuller](const auto& stepIdAndFvs) {
    return stepIdAndFvs
      .op(RxRequireNonEmpty())
      .op(RxToVector())
      .flat_map([self, stepId = stepIdAndFvs.get_key(), spiPuller](std::shared_ptr<std::vector<std::shared_ptr<FieldValue>>> fvs) {
      return self->mSurveyStepsById->observe()
        .flat_map([self, stepId, spiPuller, fvs](std::shared_ptr<SurveyStepsById> stepsById) {
        auto step = stepsById->at(stepId);
        return spiPuller->loadContent(step, fvs);
        });
      });
    });
}

rxcpp::observable<std::shared_ptr<StorableColumnContent>> SurveyAspectPuller::AllSpisPuller::loadContentForSpis(std::shared_ptr<Spis> spis, std::shared_ptr<SurveyAspectPuller> sp) {
  assert(!spis->empty());

  // Sort by date-sent-out (oldest-to-newest) so that indices will be consistent over import runs
  auto tspis = TimestampedSpi::AddTimestamps(*spis, [](std::shared_ptr<SurveyPackageInstance> spi) { return spi->getSentOn(); });
  std::sort(tspis->begin(), tspis->end(), [](const TimestampedSpi& lhs, const TimestampedSpi& rhs) {
    return lhs.getTimestamp() < rhs.getTimestamp();
  });

  return sp->getDataPoints(spis)
    .zip(
      this->getWeekNumberOffsetForParticipant(tspis->front().getSpi()->getParticipantId()),
      this->getStudyPuller()->getEnvironmentPuller()->getImportColumnNamer().op(RxGetOne("import column namer"))
    )
    .concat_map([self = SharedFrom(*this), tspis](const auto& context) {
    std::shared_ptr<SdpsBySpi> sdpsBySpi = std::get<0>(context);
    Timestamp studyStart = std::get<1>(context);
    std::shared_ptr<ImportColumnNamer> namer = std::get<2>(context);
    return rxcpp::observable<>::range(0, static_cast<int>(tspis->size() - 1U)) // .range() parameters specify first and **last**
      .concat_map([self, sdpsBySpi, tspis, studyStart, namer](int index) -> rxcpp::observable<std::shared_ptr<StorableColumnContent>> {
      const auto& tspi = tspis->at(static_cast<size_t>(index));
      auto spi = tspi.getSpi();
      auto found = sdpsBySpi->find(spi);
      if (found == sdpsBySpi->cend()) {
        return rxcpp::observable<>::empty<std::shared_ptr<StorableColumnContent>>();
      }
      auto weekno = GetWeekNumber(tspi.getTimestamp(), studyStart);
      auto spiPuller = IndexedSpiPuller::Create(namer, self->getColumnNamePrefix(), spi->getSurveyPackageName(), static_cast<unsigned>(index), weekno);
      return self->loadContentForSpi(spiPuller, rxcpp::observable<>::iterate(*found->second));
      });
    });
}

rxcpp::observable<std::shared_ptr<StorableColumnContent>> SurveyAspectPuller::LatestSpiPuller::loadContentForSpis(std::shared_ptr<Spis> spis, std::shared_ptr<SurveyAspectPuller> sp) {
  assert(!spis->empty());

  std::shared_ptr<SurveyPackageInstance> spi = spis->front();

  if (spis->size() > 1U) {
    // Reverse sort by date-finished: highest "finished" SPI will be first in the sorted vector
    auto tspis = TimestampedSpi::AddTimestamps(*spis, [](std::shared_ptr<SurveyPackageInstance> spi) { return spi->getFinishedOn(); });
    std::sort(tspis->begin(), tspis->end(), [](const TimestampedSpi& lhs, const TimestampedSpi& rhs) {
      return lhs.getTimestamp() > rhs.getTimestamp();
    });

    const auto& latest = tspis->front();
    spi = latest.getSpi();

    PULLCASTOR_LOG(info) << "Out of " << tspis->size() << " finished Survey Package Instances"
      << " for survey package " << spi->getSurveyPackageName()
      << " we'll only consider the one finished at " << latest.getTimestamp().toString();
  }

  auto self = SharedFrom(*this);
  return this->getStudyPuller()->getEnvironmentPuller()->getImportColumnNamer()
    .flat_map([self, spi, sp](std::shared_ptr<ImportColumnNamer> namer) {
    auto spiPuller = SimpleSpiPuller::Create(namer, self->getColumnNamePrefix(), spi->getSurveyPackageName());
    return self->loadContentForSpi(spiPuller, sp->getDataPoints(spi));
    });
}

rxcpp::observable<Timestamp> SurveyAspectPuller::AllSpisPuller::getWeekNumberOffsetForParticipant(const std::string& participantId) {
  return mStudyStartsByParticipantId->observe()
    .flat_map([participantId](std::shared_ptr<StudyStartsByParticipantId> ssByRecId) -> rxcpp::observable<Timestamp> {
    auto found = ssByRecId->find(participantId);
    if (found == ssByRecId->cend()) {
      PULLCASTOR_LOG(warning) << "No surveys will be imported for participant " << participantId << " because the study start cannot be determined";
      return rxcpp::observable<>::empty<Timestamp>();
    }
    return rxcpp::observable<>::just(found->second);
  });
}

rxcpp::observable<std::shared_ptr<StorableColumnContent>> SurveyAspectPuller::getStorableContent(std::shared_ptr<CastorParticipant> participant) {
  auto rawParticipant = participant->getParticipant();
  PULLCASTOR_LOG(debug) << "Getting content for study " << this->getStudyPuller()->getStudy()->getSlug() << ", surveys, participant " << rawParticipant->getId();
  return mSpisByParticipantId->observe()
    .concat_map([rawParticipant, self = SharedFrom(*this)](std::shared_ptr<SpisById> spisByParticipantId) -> rxcpp::observable<std::shared_ptr<StorableColumnContent>> { // Process SPIs for this participant ID
    auto position = spisByParticipantId->find(rawParticipant->getId());
    if (position == spisByParticipantId->cend()) {
      return rxcpp::observable<>::empty<std::shared_ptr<StorableColumnContent>>();
    }
    auto spis = position->second;
    return rxcpp::observable<>::iterate(*spis)
      .op(RxGroupToVectors([](std::shared_ptr<SurveyPackageInstance> spi) { return spi->getSurveyPackageId(); })) // Group by survey package (ID)
      .concat_map([](std::shared_ptr<SpisById> spisBySpId) {return rxcpp::observable<>::iterate(*spisBySpId); }) // Emit one vector of SPIs per survey package (ID)
      .concat_map([self](const auto& pair) { // Process SPIs for this survey package (ID)
      std::shared_ptr<Spis> spis = pair.second;
      return self->mSpisPuller->loadContentForSpis(spis, self);
    });
    });
}

}
}
