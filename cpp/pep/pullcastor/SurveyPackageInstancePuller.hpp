#pragma once

#include <pep/castor/ImportColumnNamer.hpp>
#include <pep/pullcastor/FieldValue.hpp>
#include <pep/pullcastor/StudyAspectPuller.hpp>

namespace pep {
namespace castor {

/*!
  * \brief Base class for classes that know how to import (data for steps from) a single SurveyPackageInstance instance.
  */
class SurveyPackageInstancePuller : public std::enable_shared_from_this<SurveyPackageInstancePuller>, boost::noncopyable {
private:
  std::shared_ptr<ImportColumnNamer> mNamer;
  std::string mPrefix;
  std::string mSurveyPackageName;

protected:
  explicit SurveyPackageInstancePuller(std::shared_ptr<ImportColumnNamer> namer, const std::string& prefix, const std::string& surveyPackageName);

  inline std::shared_ptr<ImportColumnNamer> getImportColumnNamer() const noexcept { return mNamer; }
  inline const std::string& getColumnNamePrefix() const noexcept { return mPrefix; }
  inline const std::string& getSurveyPackageName() const noexcept { return mSurveyPackageName; }

  /*!
  * \brief Produces the column name under which PEP should store data for the combination of this survey package instance and the specified step.
  */
  virtual std::string getColumnName(std::shared_ptr<SurveyStep> step) const = 0;

public:
  virtual ~SurveyPackageInstancePuller() = default;

  /*!
  * \brief Produces StorableColumnContent instances for the specified field values, which are associated with the specified survey step.
  * \param step The survey step associated which the specified field values.
  * \param fvs The field values to include in the StorableColumnContent.
  * \return (An observable emitting) StorableColumnContent instances, representing the data that PEP should store for the specified field values.
  * \remark The implementation in this class produces a single StorableColumnContent with JSON containing the field values' data.
  */
  virtual rxcpp::observable<std::shared_ptr<StorableColumnContent>> loadContent(
    std::shared_ptr<SurveyStep> step,
    std::shared_ptr<std::vector<std::shared_ptr<FieldValue>>> fvs) const;
};


class SimpleSpiPuller : public SurveyPackageInstancePuller, public SharedConstructor<SimpleSpiPuller> {
  friend class SharedConstructor<SimpleSpiPuller>;

private:
  explicit SimpleSpiPuller(std::shared_ptr<ImportColumnNamer> namer, const std::string& prefix, const std::string& surveyPackageName);

protected:
  /*!
  * \brief Produces the column name under which PEP should store data for the combination of this survey package instance and the specified step.
  */
  std::string getColumnName(std::shared_ptr<SurveyStep> step) const override;
};


class IndexedSpiPuller : public SurveyPackageInstancePuller, public SharedConstructor<IndexedSpiPuller> {
  friend class SharedConstructor<IndexedSpiPuller>;

private:
  unsigned mIndex;
  int mWeekNumber;

  IndexedSpiPuller(std::shared_ptr<ImportColumnNamer> namer, const std::string& prefix, const std::string& surveyPackageName, unsigned index, int weekNumber);

protected:
  /*!
  * \brief Produces the column name under which PEP should store data for the combination of this survey package instance and the specified step.
  */
  std::string getColumnName(std::shared_ptr<SurveyStep> step) const override;

public:
  /*!
  * \brief Produces StorableColumnContent instances for the specified field values, which are associated with the specified survey step.
  * \param step The survey step associated which the specified field values.
  * \param fvs The field values to include in the StorableColumnContent.
  * \return (An observable emitting) StorableColumnContent instances, representing the data that PEP should store for the specified field values.
  * \remark The implementation in this class produces the base class's StorableColumnContent (with JSON containing the field values' data), as well as data for a corresponding ".WeekNumber" column.
  */
  rxcpp::observable<std::shared_ptr<StorableColumnContent>> loadContent(
    std::shared_ptr<SurveyStep> step,
    std::shared_ptr<std::vector<std::shared_ptr<FieldValue>>> fvs) const override;
};

}
}
