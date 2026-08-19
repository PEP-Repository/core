#include <pep/pullcastor/CrfAspectPuller.hpp>
#include <pep/pullcastor/RepeatingDataAspectPuller.hpp>
#include <pep/pullcastor/SurveyAspectPuller.hpp>

#include <pep/async/RxIterate.hpp>

#include <boost/property_tree/json_parser.hpp>

#include <rxcpp/operators/rx-map.hpp>

namespace pep {
namespace castor {

StudyAspectPuller::StudyAspectPuller(std::shared_ptr<StudyPuller> study, const StudyAspect& aspect)
  : mStudy(study), mSpColumn(aspect.getShortPseudonymColumn()), mColumnNamePrefix(aspect.getStorage()->getDataColumn()) {
}

rxcpp::observable<std::shared_ptr<StudyAspectPuller>> StudyAspectPuller::CreateChildrenFor(std::shared_ptr<StudyPuller> study) {
  return RxIterate(*study->getAspects())
    .map([study](const StudyAspect& aspect) -> std::shared_ptr<StudyAspectPuller> {
      auto type = aspect.getStorage()->getStudyType();
      switch (type) {
      case CastorStudyType::STUDY:
        return CrfAspectPuller::Create(study, aspect);
      case CastorStudyType::REPEATING_DATA:
        return RepeatingDataAspectPuller::Create(study, aspect);
      case CastorStudyType::SURVEY:
        return SurveyAspectPuller::Create(study, aspect);
      }
      auto msg = "Unsupported study type " + std::to_string(type);
      PULLCASTOR_LOG(debug) << msg;
      throw std::runtime_error(msg);
    });
}

}
}
