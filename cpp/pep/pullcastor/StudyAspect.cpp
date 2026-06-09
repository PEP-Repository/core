#include <pep/pullcastor/StudyAspect.hpp>

#include <pep/async/RxIterate.hpp>

#include <rxcpp/operators/rx-filter.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>

namespace pep {
namespace castor {

StudyAspect::StudyAspect(const std::string& slug, const std::string& spColumn, std::shared_ptr<CastorStorageDefinition> storage)
  : mSlug(slug), mSpColumn(spColumn), mStorage(storage) {
}

rxcpp::observable<StudyAspect> StudyAspect::GetAll(rxcpp::observable<ShortPseudonymDefinition> sps) {
  return sps
    .filter([](const ShortPseudonymDefinition& sp) {return sp.getCastor().has_value(); })
    .flat_map([](const ShortPseudonymDefinition& sp) {
    auto column = sp.getColumn().getFullName();
    const auto& castor = *sp.getCastor();
    const auto& defaultSlug = castor.getStudySlug();
    return RxIterate(castor.getStorageDefinitions())
      .map([column, defaultSlug](std::shared_ptr<CastorStorageDefinition> storage) {
      auto slug = storage->getImportStudySlug();
      if (slug.empty()) {
        slug = defaultSlug;
      }
      return StudyAspect(slug, column, storage);
      });
    });
}

}
}
