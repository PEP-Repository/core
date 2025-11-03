#include <pep/async/RxMoveIterate.hpp>
#include <pep/async/RxRequireCount.hpp>
#include <pep/async/RxToUnorderedMap.hpp>
#include <pep/async/RxToVector.hpp>
#include <pep/castor/RepeatingDataPoint.hpp>
#include <pep/pullcastor/CastorParticipant.hpp>
#include <pep/pullcastor/StudyAspectPuller.hpp>

#include <rxcpp/operators/rx-concat_map.hpp>
#include <rxcpp/operators/rx-filter.hpp>
#include <rxcpp/operators/rx-group_by.hpp>
#include <rxcpp/operators/rx-on_error_resume_next.hpp>
#include <rxcpp/operators/rx-take.hpp>

namespace pep {
namespace castor {

namespace {

rxcpp::observable<std::shared_ptr<CastorParticipant>> GetKnownParticipants(
  rxcpp::observable<std::shared_ptr<StoredData>> stored,
  std::shared_ptr<std::vector<std::shared_ptr<CastorParticipant>>> allParticipants,
  std::shared_ptr<StudyAspectPuller> aspect) {
  return stored
    .flat_map([allParticipants, spColumn = aspect->getShortPseudonymColumn()](std::shared_ptr<StoredData> stored) {
    return RxMoveIterate(*allParticipants) // ...for every participant...
      .filter([stored, spColumn](std::shared_ptr<CastorParticipant> participant) { // ...if participant corresponds with an SP known by PEP...
      auto id = participant->getParticipant()->getId();
      ColumnBoundParticipantId cbpId(spColumn, id);
      auto result = stored->hasCastorParticipantId(cbpId);
      if (!result) {
        PULLCASTOR_LOG(debug) << "Skipping participant " << id << ", which is not registered in PEP column " << spColumn;
      }
      return result;
      });
    });
}

}

StudyPuller::StudyPuller(std::shared_ptr<EnvironmentPuller> environment, std::shared_ptr<Study> study, std::shared_ptr<std::vector<StudyAspect>> aspects)
  : mEnvironment(environment), mStudy(study), mAspects(aspects) {
  PULLCASTOR_LOG(debug) << "Creating puller for study " << this->getStudy()->getSlug();
  mParticipants = CreateRxCache([environment, study]() {
    return study->getParticipants()
      .filter([environment](std::shared_ptr<Participant> participant) {
      const auto& allowed = environment->getShortPseudonymsToProcess();
      if (!allowed.has_value()) {
        return true;
      }
      return std::find(allowed->cbegin(), allowed->cend(), participant->getId()) != allowed->cend();
        });
    });

  // Bulk-retrieve and cache objects related to repeating data if we're processing all participants
  if (!environment->getShortPseudonymsToProcess().has_value()) {
    mRepeatingDataInstances = CreateRxCache([study, participants = mParticipants]() {
      return RepeatingDataInstance::BulkRetrieve(study, participants->observe())
        .on_error_resume_next(&RepeatingDataInstance::ConvertNotFoundToEmpty);
      });
    mRepeatingDataPoints = CreateRxCache([study, rdis = mRepeatingDataInstances]() {
      return RepeatingDataPoint::BulkRetrieve(study, rdis->observe());
      });
  }

  mFields = CreateRxCache([study]() {return study->getFields(); });
  mFieldsById = CreateRxCache([fields = mFields]() {
    return fields->observe()
      .op(RxToUnorderedMap([](std::shared_ptr<Field> field) {return field->getId(); }));
    });
  mRepeatingDataPullers = CreateRxCache([study, fields = mFields]() {
    return fields->observe()
      .op(RxToVector())
      .flat_map([study](std::shared_ptr<std::vector<std::shared_ptr<Field>>> allFields) {
      return study->getRepeatingData()
        .map([allFields](std::shared_ptr<RepeatingData> repeatingData) {return RepeatingDataPuller::Create(repeatingData, allFields); })
        .op(RxToUnorderedMap([](std::shared_ptr<RepeatingDataPuller> rdp) {return rdp->getRepeatingData()->getId(); }));
      });
    });
}

rxcpp::observable<std::shared_ptr<StudyPuller>> StudyPuller::CreateChildrenFor(std::shared_ptr<EnvironmentPuller> environment) {
  return environment->getStudyAspects()
    .group_by([](const StudyAspect& aspect) {return aspect.getSlug(); })
    .flat_map([environment](rxcpp::grouped_observable<std::string, StudyAspect> group) {
    auto slug = group.get_key();
    return group
      .op(RxRequireNonEmpty())
      .op(RxToVector())
      .flat_map([environment, slug](std::shared_ptr<std::vector<StudyAspect>> aspects) {
      return environment->getStudyBySlug(slug)
        .map([environment, aspects](std::shared_ptr<Study> study) {return StudyPuller::Create(environment, study, aspects); });
      });
    });
}

rxcpp::observable<std::shared_ptr<StorableCellContent>> StudyPuller::getStorableContent() {
  auto self = SharedFrom(*this);

  return mParticipants->observe()
    .map([self](std::shared_ptr<Participant> participant) {return CastorParticipant::Create(self, participant); }) // Create caching object for this participant
    .op(RxToVector()) // Ensure that participants can be iterated over multiple times (for multiple aspects)
    .concat_map([self](std::shared_ptr<std::vector<std::shared_ptr<CastorParticipant>>> participants) {
    return StudyAspectPuller::CreateChildrenFor(self) // For every aspect...
      .concat_map([self, participants](std::shared_ptr<StudyAspectPuller> aspect) {
      return GetKnownParticipants(self->getEnvironmentPuller()->getStoredData(), participants, aspect) // ...for every participant known for this aspect...
        .concat_map([aspect](std::shared_ptr<CastorParticipant> participant) {
        return aspect->getStorableContent(participant) // ...retrieve the participant's aspect's data from Castor...
          .map([cbpId = ColumnBoundParticipantId(aspect->getShortPseudonymColumn(), participant->getParticipant()->getId())](std::shared_ptr<StorableColumnContent> col) { // ... and return it as a StorableCellContent
          return StorableCellContent::Create(cbpId, col->getColumn(), col->getContent(), col->getFileExtension());
          });
        });
      });
    });
}

rxcpp::observable<std::shared_ptr<RepeatingDataInstance>> StudyPuller::getRepeatingDataInstances(std::shared_ptr<Participant> participant) {
  if (mRepeatingDataInstances != nullptr) {
    return mRepeatingDataInstances->observe()
      .filter([participant](std::shared_ptr<RepeatingDataInstance> rdi) {return rdi->getParticipant() == participant; });
  }
  return participant->getRepeatingDataInstances();
}

rxcpp::observable<std::shared_ptr<RepeatingDataPoint>> StudyPuller::getRepeatingDataPoints(std::shared_ptr<RepeatingDataInstance> rdi) {
  if (mRepeatingDataPoints != nullptr) {
    return mRepeatingDataPoints->observe()
      .filter([rdi](std::shared_ptr<RepeatingDataPoint> rdp) {return rdp->getRepeatingDataInstance() == rdi; });
  }
  return rdi->getRepeatingDataPoints();
}

rxcpp::observable<std::shared_ptr<Field>> StudyPuller::getFields() {
  return mFieldsById->observe()
    .flat_map([](std::shared_ptr<FieldsById> byId) {return RxMoveIterate(*byId); })
    .map([](const auto& pair) {return pair.second; });
}

rxcpp::observable<std::shared_ptr<RepeatingDataPuller>> StudyPuller::getRepeatingDataPullers() {
  return mRepeatingDataPullers->observe()
    .flat_map([](auto byId) {return RxMoveIterate(*byId); })
    .map([](const auto& pair) {return pair.second; });
}

rxcpp::observable<std::shared_ptr<RepeatingDataPuller>> StudyPuller::getRepeatingDataPuller(const std::string& repeatingDataId) {
  return mRepeatingDataPullers->observe()
    .map([repeatingDataId](auto byId) {return byId->at(repeatingDataId); });
}

rxcpp::observable<std::shared_ptr<FieldValue>> StudyPuller::toFieldValue(std::shared_ptr<DataPointBase> dp) {
  return mFieldsById->observe()
    .op(RxGetOne("Fields by ID"))
    .map([dp](std::shared_ptr<FieldsById> byId) {
    auto field = byId->at(dp->getId());
    return std::make_shared<FieldValue>(field, dp);
    });
}

}
}
