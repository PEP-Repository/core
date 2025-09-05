#include <pep/castor/Study.hpp>
#include <pep/pullcastor/PullCastorUtils.hpp>
#include <pep/pullcastor/TimestampedSpi.hpp>

namespace pep {
namespace castor {

TimestampedSpi::TimestampedSpi(std::shared_ptr<SurveyPackageInstance> spi, const Timestamp& completionTimestamp)
  : mSpi(spi), mTimestamp(completionTimestamp) {
}

std::shared_ptr<std::vector<TimestampedSpi>> TimestampedSpi::AddTimestamps(const std::vector<std::shared_ptr<SurveyPackageInstance>> spis, const GetTimestampProperties& getTsProps) {
  auto result = std::make_shared<std::vector<TimestampedSpi>>();
  result->reserve(spis.size());
  for (auto spi : spis) {
    auto props = getTsProps(spi);
    if (props) {
      auto timestamp = ParseCastorDateTime(*props);
      result->push_back(TimestampedSpi(spi, timestamp));
    }
  }
  return result;
}

}
}
