#include <pep/pullcastor/CrfAspectPuller.hpp>
#include <pep/pullcastor/SurveyAspectPuller.hpp>

#include <pep/async/RxIterate.hpp>

#include <boost/property_tree/json_parser.hpp>

#include <rxcpp/operators/rx-map.hpp>

namespace pep {
namespace castor {

std::unordered_map<CastorStudyType, StudyAspectPuller::CreateFunction>& StudyAspectPuller::GetCreateFunctions() {
  static std::unordered_map<CastorStudyType, CreateFunction> result;
  return result;
}

StudyAspectPuller::StudyAspectPuller(std::shared_ptr<StudyPuller> study, const StudyAspect& aspect)
  : mStudy(study), mSpColumn(aspect.getShortPseudonymColumn()), mColumnNamePrefix(aspect.getStorage()->getDataColumn()) {
}

rxcpp::observable<std::shared_ptr<StudyAspectPuller>> StudyAspectPuller::CreateChildrenFor(std::shared_ptr<StudyPuller> study) {
  return RxIterate(*study->getAspects())
    .map([study](const StudyAspect& aspect) {
    const auto& creators = GetCreateFunctions();
    auto type = aspect.getStorage()->getStudyType();
    auto position = creators.find(type);
    if (position == creators.cend()) {
      auto msg = "Unsupported study type " + std::to_string(type);
      PULLCASTOR_LOG(debug) << msg;
      throw std::runtime_error(msg);
    }
    return position->second(study, aspect);
    });
}

}
}
