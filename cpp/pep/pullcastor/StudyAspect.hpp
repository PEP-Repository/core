#pragma once

#include <pep/structure/ShortPseudonyms.hpp>
#include <rxcpp/rx-lite.hpp>

namespace pep {
namespace castor {

/*!
  * \brief Represents a study aspect that should be imported from castor, e.g. "the SURVEYS from study XYZ".
  * \remark This class only provides (configuration) data. The import of study aspects is handled by StudyAspectPuller and derived classes.
  */
class StudyAspect {
private:
  std::string mSlug;
  std::string mSpColumn;
  std::shared_ptr<CastorStorageDefinition> mStorage;

  StudyAspect(const std::string& slug, const std::string& spColumn, std::shared_ptr<CastorStorageDefinition> storage);

public:
  /*!
  * \brief Produces all study aspects that should be pulled from Castor.
  * \param sps The short pseudonyms to process.
  * \return (An observable emitting) all StudyAspect instances that should be pulled.
  */
  static rxcpp::observable<StudyAspect> GetAll(rxcpp::observable<ShortPseudonymDefinition> sps);

  /*!
  * \brief Produces the slug of the Castor study that data should be pulled from.
  * \return The slug of the Castor study to import data from.
  */
  inline const std::string& getSlug() const noexcept { return mSlug; }

  /*!
  * \brief Produces the name of the PEP column containing short pseudonyms that correspond with Castor participant IDs.
  * \return The name of the PEP short pseudonym column.
  */
  inline const std::string& getShortPseudonymColumn() const noexcept { return mSpColumn; }

  /*!
  * \brief Produces the CastorStorageDefinition associated with the study aspect.
  * \return A CastorStorageDefinition extracted from GlobalConfiguration.
  */
  inline std::shared_ptr<CastorStorageDefinition> getStorage() const noexcept { return mStorage; }
};

}
}
