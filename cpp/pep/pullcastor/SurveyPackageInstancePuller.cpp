#include <pep/pullcastor/SurveyPackageInstancePuller.hpp>

#include <pep/async/RxIterate.hpp>

#include <boost/property_tree/ptree.hpp>
#include <rxcpp/operators/rx-concat.hpp>

namespace pep {
namespace castor {

SurveyPackageInstancePuller::SurveyPackageInstancePuller(std::shared_ptr<ImportColumnNamer> namer, const std::string& prefix, const std::string& surveyPackageName)
  : mNamer(namer), mPrefix(prefix), mSurveyPackageName(surveyPackageName) {
}

rxcpp::observable<std::shared_ptr<StorableColumnContent>> SurveyPackageInstancePuller::loadContent(
  std::shared_ptr<SurveyStep> step,
  std::shared_ptr<std::vector<std::shared_ptr<FieldValue>>> fvs) const {
  auto column = this->getColumnName(step);
  return StorableColumnContent::CreateJson(column, RxIterate(*fvs), std::make_shared<boost::property_tree::ptree>());
}

SimpleSpiPuller::SimpleSpiPuller(std::shared_ptr<ImportColumnNamer> namer, const std::string& prefix, const std::string& surveyPackageName)
  : SurveyPackageInstancePuller(namer, prefix, surveyPackageName) {
}

std::string SimpleSpiPuller::getColumnName(std::shared_ptr<SurveyStep> step) const {
  return this->getImportColumnNamer()->getColumnName(this->getColumnNamePrefix(), this->getSurveyPackageName(), step);
}

IndexedSpiPuller::IndexedSpiPuller(std::shared_ptr<ImportColumnNamer> namer, const std::string& prefix, const std::string& surveyPackageName, unsigned index, int weekNumber)
  : SurveyPackageInstancePuller(namer, prefix, surveyPackageName), mIndex(index), mWeekNumber(weekNumber) {
}

std::string IndexedSpiPuller::getColumnName(std::shared_ptr<SurveyStep> step) const {
  return this->getImportColumnNamer()->getColumnName(this->getColumnNamePrefix(), this->getSurveyPackageName(), step, mIndex);
}

rxcpp::observable<std::shared_ptr<StorableColumnContent>> IndexedSpiPuller::loadContent(
  std::shared_ptr<SurveyStep> step,
  std::shared_ptr<std::vector<std::shared_ptr<FieldValue>>> fvs) const {
  auto column = this->getImportColumnNamer()->getWeekNumberColumnName(this->getColumnNamePrefix(), this->getSurveyPackageName(), step, mIndex);
  auto content = PreloadedCellContent::Create(std::to_string(mWeekNumber));
  auto weekno = StorableColumnContent::Create(column, content, ".txt");

  return SurveyPackageInstancePuller::loadContent(step, fvs) // Return (JSON) payload for this step
    .concat(rxcpp::observable<>::just(weekno)); // Return week number column data for this step
}

}
}
