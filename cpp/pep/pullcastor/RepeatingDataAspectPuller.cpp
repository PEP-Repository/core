#include <pep/async/RxRequireCount.hpp>
#include <pep/async/RxToVector.hpp>
#include <pep/pullcastor/RepeatingDataAspectPuller.hpp>
#include <boost/property_tree/ptree.hpp>

namespace pep {
namespace castor {

RepeatingDataAspectPuller::RepeatingDataAspectPuller(std::shared_ptr<StudyPuller> sp, const StudyAspect& aspect)
  : TypedStudyAspectPuller<RepeatingDataAspectPuller, CastorStudyType::REPEATING_DATA>(sp, aspect) {
  mRepeatingDataPullers = CreateRxCache([sp]() { return sp->getRepeatingDataPullers().op(RxToVector()); });
}

rxcpp::observable<std::shared_ptr<StorableColumnContent>> RepeatingDataAspectPuller::getStorableContent(std::shared_ptr<CastorParticipant> participant) {
  return mRepeatingDataPullers->observe()
    .flat_map([sp = this->getStudyPuller(), participant](auto rdps) {
    return RepeatingDataPuller::Aggregate(sp, rdps, participant->getRepeatingDataInstances())
      .op(RxGetOne("reports tree"));
    })
    .flat_map([column = this->getColumnNamePrefix()](std::shared_ptr<boost::property_tree::ptree> tree) -> rxcpp::observable<std::shared_ptr<StorableColumnContent>> {
      if (tree->empty()) {
        return rxcpp::observable<>::empty<std::shared_ptr<StorableColumnContent>>();
      }
      return StorableColumnContent::CreateJson(column, rxcpp::observable<>::empty<std::shared_ptr<FieldValue>>(), tree);
    });
}

}
}
