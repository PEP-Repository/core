#include <pep/async/RxGroupToVectors.hpp>
#include <pep/async/RxMoveIterate.hpp>
#include <pep/async/RxRequireCount.hpp>
#include <pep/async/RxSharedPtrCast.hpp>
#include <pep/async/RxToUnorderedMap.hpp>
#include <pep/async/RxToVector.hpp>
#include <pep/castor/StudyDataPoint.hpp>
#include <pep/pullcastor/CrfAspectPuller.hpp>

#include <rxcpp/operators/rx-concat_map.hpp>
#include <rxcpp/operators/rx-filter.hpp>

namespace pep {
namespace castor {

class CrfAspectPuller::FormPuller : public std::enable_shared_from_this<FormPuller>, private SharedConstructor<FormPuller> {
  friend SharedConstructor<FormPuller>;

private:
  std::string mFormId;
  std::string mColumnName;
  std::shared_ptr<std::vector<std::shared_ptr<RepeatingDataPuller>>> mRepeatingDataPullers;

  FormPuller(const std::string& formId, const std::string& columnName, std::shared_ptr<std::vector<std::shared_ptr<RepeatingDataPuller>>> repeatingDataPullers);

public:
  static rxcpp::observable<std::shared_ptr<FormPuller>> LoadAll(std::shared_ptr<StudyPuller> sp, const std::string& columnPrefix);

  inline const std::string& getFormId() const noexcept { return mFormId; }

  rxcpp::observable<std::shared_ptr<StorableColumnContent>> loadContentFromCastor(std::shared_ptr<StudyPuller> sp, std::shared_ptr<CastorParticipant> participant, std::shared_ptr<FieldValues> fvs);
};

rxcpp::observable<std::shared_ptr<CrfAspectPuller::FormPuller>> CrfAspectPuller::FormPuller::LoadAll(std::shared_ptr<StudyPuller> sp, const std::string& columnPrefix) {
  return sp->getEnvironmentPuller()->getImportColumnNamer()
    .flat_map([sp, columnPrefix](std::shared_ptr<ImportColumnNamer> namer) {
    return sp->getStudy()->getForms()
      .flat_map([sp, columnPrefix, namer](std::shared_ptr<Form> form) {
      auto formId = form->getId();
      return sp->getFields()
        .filter([formId](std::shared_ptr<Field> field) { return field->getParentId() == formId && field->getType() == Field::TYPE_REPEATED_MEASURE; })
        .flat_map([sp](std::shared_ptr<Field> field) {return sp->getRepeatingDataPuller(field->getReportId()); })
        .op(RxToVector())
        .map([formId, columnName = namer->getColumnName(columnPrefix, form)](std::shared_ptr<std::vector<std::shared_ptr<RepeatingDataPuller>>> repeatingDataPullers) {
          return FormPuller::Create(formId, columnName, repeatingDataPullers);
        });
      });
    });
}

CrfAspectPuller::FormPuller::FormPuller(const std::string& formId, const std::string& columnName, std::shared_ptr<std::vector<std::shared_ptr<RepeatingDataPuller>>> repeatingDataPullers)
  : mFormId(formId), mColumnName(columnName), mRepeatingDataPullers(repeatingDataPullers) {
}

rxcpp::observable<std::shared_ptr<StorableColumnContent>> CrfAspectPuller::FormPuller::loadContentFromCastor(std::shared_ptr<StudyPuller> sp, std::shared_ptr<CastorParticipant> participant, std::shared_ptr<FieldValues> fvs) {
  return RepeatingDataPuller::Aggregate(sp, mRepeatingDataPullers, participant->getRepeatingDataInstances())
    .op(RxGetOne("reports tree"))
    .flat_map([fvs, column = mColumnName](std::shared_ptr<boost::property_tree::ptree> reports) {
    return StorableColumnContent::CreateJson(column, RxMoveIterate(*fvs), reports);
    })
    .op(RxGetOne("CRF form cell data"));
}

CrfAspectPuller::CrfAspectPuller(std::shared_ptr<StudyPuller> sp, const StudyAspect& aspect)
  : TypedStudyAspectPuller<CrfAspectPuller, CastorStudyType::STUDY>(sp, aspect), mImmediatePartialData(aspect.getStorage()->immediatePartialData()) {
  mFormPullers = CreateRxCache([sp, prefix = this->getColumnNamePrefix()]() {
    return FormPuller::LoadAll(sp, prefix)
      .op(RxToUnorderedMap([](std::shared_ptr<FormPuller> form) {return form->getFormId(); }));
  });
  // Bulk-retrieve and cache SDP data if we're processing all participants
  if (!sp->getEnvironmentPuller()->getShortPseudonymsToProcess().has_value()) {
    mSdpsByParticipant = CreateRxCache([sp]() {
      return StudyDataPoint::BulkRetrieve(sp->getStudy(), sp->getParticipants())
        .op(RxGroupToVectors([](std::shared_ptr<StudyDataPoint> sdp) {return sdp->getParticipant(); }));
      });
  }
}

rxcpp::observable<std::shared_ptr<StudyDataPoint>> CrfAspectPuller::getStudyDataPoints(std::shared_ptr<Participant> participant) {
  // Return cached data if we have it
  if (mSdpsByParticipant != nullptr) {
    return mSdpsByParticipant->observe()
      .concat_map([participant](std::shared_ptr<StudyDataPointsByParticipant> sdpsByParticipant) -> rxcpp::observable<std::shared_ptr<StudyDataPoint>> {
      auto position = sdpsByParticipant->find(participant);
      if (position == sdpsByParticipant->cend()) {
        return rxcpp::observable<>::empty<std::shared_ptr<StudyDataPoint>>();
      }
      return RxMoveIterate(*position->second);
        });
  }

  // Retrieve directly from API if we didn't have a cache
  return participant->getStudyDataPoints();
}

rxcpp::observable<std::shared_ptr<StorableColumnContent>> CrfAspectPuller::loadFormContentFromCastor(std::shared_ptr<CastorParticipant> participant, const std::string& formId, std::shared_ptr<FieldValues> fvs) {
  assert(!fvs->empty());

  return mFormPullers->observe()
    .flat_map([sp = this->getStudyPuller(), participant, formId, fvs](std::shared_ptr<FormPullersByFormId> byId) {
    auto puller = byId->at(formId);
    return puller->loadContentFromCastor(sp, participant, fvs);
    });
}

rxcpp::observable<std::shared_ptr<StorableColumnContent>> CrfAspectPuller::getStorableContent(std::shared_ptr<CastorParticipant> participant){
  auto slug = this->getStudyPuller()->getStudy()->getSlug();
  auto rawParticipant = participant->getParticipant();
  auto id = rawParticipant->getId();

  if (!mImmediatePartialData) {
    if (rawParticipant->getProgress() < 100 && !rawParticipant->isLocked()) {
      PULLCASTOR_LOG(debug) << "Skipping study " << slug << "'s CRF for participant " << id << ", which is not completed";
      return rxcpp::rxs::empty<std::shared_ptr<StorableColumnContent>>();
    }
    auto updatedOn = ParseCastorDateTime(rawParticipant->getUpdatedOn());
    if (updatedOn >= this->getStudyPuller()->getEnvironmentPuller()->getCooldownThreshold()) {
      PULLCASTOR_LOG(debug) << "Skipping study " << slug << "'s CRF for participant " << id << ", which has been updated too recently.";
      return rxcpp::rxs::empty<std::shared_ptr<StorableColumnContent>>();
    }
  }

  PULLCASTOR_LOG(debug) << "Loading study " << slug << "'s CRF for participant " << id << " from Castor";
  return this->getStudyDataPoints(rawParticipant)
    .op(RxSharedPtrCast<DataPointBase>())
    .flat_map([sp = this->getStudyPuller()](std::shared_ptr<DataPointBase> dp) {return sp->toFieldValue(dp).op(RxGetOne("CRF field value")); })
    .group_by([](std::shared_ptr<FieldValue> fv) {return fv->getField()->getParentId(); })
    .flat_map([self = SharedFrom(*this), participant](rxcpp::grouped_observable<std::string, std::shared_ptr<FieldValue>> formIdAndFvs) {
    return formIdAndFvs
      .op(RxRequireNonEmpty())
      .op(RxToVector())
      .flat_map([self, participant, formId = formIdAndFvs.get_key()](std::shared_ptr<FieldValues> fvs) {return self->loadFormContentFromCastor(participant, formId, fvs); });
    });
}

}
}
