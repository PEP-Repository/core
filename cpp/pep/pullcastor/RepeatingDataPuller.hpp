#pragma once

#include <pep/async/FakeVoid.hpp>
#include <pep/async/RxCache.hpp>
#include <pep/castor/RepeatingData.hpp>
#include <pep/castor/RepeatingDataInstance.hpp>
#include <pep/pullcastor/FieldValue.hpp>

namespace pep {
namespace castor {

class StudyPuller;

/*!
  * \brief Pulls Castor RepeatingData(Instance) data. A RepeatingDataPuller is associated with a single castor::RepeatingData (type, definition) instance.
  */
class RepeatingDataPuller : public std::enable_shared_from_this<RepeatingDataPuller>, public SharedConstructor<RepeatingDataPuller> {
  friend class SharedConstructor<RepeatingDataPuller>;

private:
  std::shared_ptr<RepeatingData> mRepeatingData;
  std::shared_ptr<RxCache<std::shared_ptr<Field>>> mFields;

  RepeatingDataPuller(std::shared_ptr<RepeatingData> repeatingData, std::shared_ptr<std::vector<std::shared_ptr<Field>>> allFields);

  /*!
   * \brief From a set of candidate RepeatingDataInstances, if those RepeatingDataInstances belong to this puller's RepeatingData, add the instance values to the specified destination ptree.
   */
  rxcpp::observable<FakeVoid> addMatchingInstancesTo(std::shared_ptr<StudyPuller> sp, std::shared_ptr<boost::property_tree::ptree> destination, rxcpp::observable<std::shared_ptr<RepeatingDataInstance>> candidates);
  rxcpp::observable<std::shared_ptr<FieldValue>> getRepeatingDataInstanceFieldValues(std::shared_ptr<StudyPuller> sp, std::shared_ptr<RepeatingDataInstance> rdi);

public:
  /*!
  * \brief Produces the RepeatingData (type, definition) instance associated with this object.
  * \return A castor::RepeatingData instance.
  */
  inline std::shared_ptr<RepeatingData> getRepeatingData() const noexcept { return mRepeatingData; }

  /*!
  * \brief Produces (an observable emitting) the Field instances associated with this repeating data (type, definition).
  * \return (An observable emitting) Field instances.
  * \remark Data are retrieved from Castor only once. Subsequent calls of this method are served from cached data.
  */
  inline rxcpp::observable<std::shared_ptr<Field>> getFields() { return mFields->observe(); }

  /*!
  * \brief Collects data for the specified RepeatingDataPuller instances into a Ptree.
  * \param sp The StudyPuller for which we're aggregating.
  * \param rdps The RepeatingDataPullers for the RepeatingData (types, definitions) to include in the Ptree.
  * \param candidates RepeatingDataInstances to process. Each RepeatingDataInstance will only be included in the resulting PTree if it matches one of the specified RepeatingDataPuller objects.
  * \return (An observable emitting) a single PTree instance.
  */
  static rxcpp::observable<std::shared_ptr<boost::property_tree::ptree>> Aggregate(
    std::shared_ptr<StudyPuller> sp,
    std::shared_ptr<std::vector<std::shared_ptr<RepeatingDataPuller>>> rdps,
    rxcpp::observable<std::shared_ptr<RepeatingDataInstance>> candidates);
};

}
}
