#pragma once

#include <pep/crypto/Timestamp.hpp>
#include <pep/castor/SurveyPackageInstance.hpp>

namespace pep {
namespace castor {

/*!
  * \brief Associates a SurveyPackageInstance with a timestamp extracted from (a PTree node in) said SurveyPackageInstance.
  */
class TimestampedSpi {
public:
  using GetTimestampProperties = std::function<std::shared_ptr<const boost::property_tree::ptree>(std::shared_ptr<SurveyPackageInstance>)>;

private:
  std::shared_ptr<SurveyPackageInstance> mSpi;
  Timestamp mTimestamp;

  TimestampedSpi(std::shared_ptr<SurveyPackageInstance> spi, const Timestamp& timestamp);

public:
  /*!
  * \brief Produces the SurveyPackageInstance associated with this object.
  * \return A (shared_ptr to a) SurveyPackageInstance object.
  */
  inline std::shared_ptr<SurveyPackageInstance> getSpi() const noexcept { return mSpi; }

  /*!
  * \brief Produces the Timestamp associated with this object.
  * \return A Timestamp object.
  */
  inline const Timestamp& getTimestamp() const noexcept { return mTimestamp; }

  /*!
  * \brief Associates a number of SurveyPackageInstance objects with Timestamps extracted from those SurveyPackageInstance objects.
  * \param spis The SurveyPackageInstance objects to process.
  * \param getTsProps A function returning the PTree containing the relevant Date+Time properties produced by the Castor API.
  * \return (A vector containing) TimestampedSpi objects corresponding with the specified SurveyPackageInstance objects.
  */
  static std::shared_ptr<std::vector<TimestampedSpi>> AddTimestamps(const std::vector<std::shared_ptr<SurveyPackageInstance>> spis, const GetTimestampProperties& getTsProps);
};

}
}
