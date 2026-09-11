#include <pep/pullcastor/CrfAspectPuller.hpp>
#include <pep/pullcastor/RepeatingDataAspectPuller.hpp>
#include <pep/pullcastor/SurveyAspectPuller.hpp>

#include <pep/async/RxIterate.hpp>

#include <boost/property_tree/json_parser.hpp>

#include <rxcpp/operators/rx-map.hpp>

namespace pep {
namespace castor {

StudyAspectPuller::StudyAspectPuller(std::shared_ptr<StudyPuller> study, const StudyAspect& aspect)
  : study_(study), spColumn_(aspect.getShortPseudonymColumn()), columnNamePrefix_(aspect.getStorage()->getDataColumn()) {
}

rxcpp::observable<std::shared_ptr<StudyAspectPuller>> StudyAspectPuller::CreateChildrenFor(std::shared_ptr<StudyPuller> study) {
  return RxIterate(*study->getAspects())
    .map([study](const StudyAspect& aspect) -> std::shared_ptr<StudyAspectPuller> {
      auto type = aspect.getStorage()->getStudyType();
      switch (type) {
      case CastorStudyType::Crf:
        return CrfAspectPuller::Create(study, aspect);
      case CastorStudyType::RepeatingData:
        return RepeatingDataAspectPuller::Create(study, aspect);
      case CastorStudyType::Survey:
        return SurveyAspectPuller::Create(study, aspect);
      }
      auto msg = "Unsupported study type " + std::to_string(ToUnderlying(type));
      PEP_PULLCASTOR_LOG(Severity::Debug) << msg;
      throw std::runtime_error(msg);
    });
}

}
}
