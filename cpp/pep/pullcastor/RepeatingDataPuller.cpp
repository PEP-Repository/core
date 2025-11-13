#include <pep/async/RxInstead.hpp>
#include <pep/async/RxIterate.hpp>
#include <pep/async/RxToUnorderedMap.hpp>
#include <pep/async/RxToVector.hpp>
#include <pep/castor/RepeatingDataPoint.hpp>
#include <pep/castor/RepeatingDataForm.hpp>
#include <pep/pullcastor/FieldValue.hpp>
#include <pep/pullcastor/StudyPuller.hpp>

#include <boost/property_tree/ptree.hpp>

#include <rxcpp/operators/rx-concat_map.hpp>
#include <rxcpp/operators/rx-filter.hpp>

namespace pep {
namespace castor {

RepeatingDataPuller::RepeatingDataPuller(std::shared_ptr<RepeatingData> repeatingData, std::shared_ptr<std::vector<std::shared_ptr<Field>>> allFields)
  : mRepeatingData(repeatingData) {
  assert(!allFields->empty());

  mFields = CreateRxCache([repeatingData, allFields]() {
    return repeatingData->getRepeatingDataForms()
      .map([](std::shared_ptr<RepeatingDataForm> form) {return form->getId(); })
      .op(RxToVector())
      .flat_map([allFields](std::shared_ptr<std::vector<std::string>> formIds) {
      return RxIterate(*allFields)
        .filter([formIds](std::shared_ptr<Field> field) {
        auto end = formIds->cend();
        return std::find(formIds->cbegin(), end, field->getParentId()) != end;
        });
      });
  });
}

rxcpp::observable<std::shared_ptr<FieldValue>> RepeatingDataPuller::getRepeatingDataInstanceFieldValues(std::shared_ptr<StudyPuller> sp, std::shared_ptr<RepeatingDataInstance> rdi) {
  assert(rdi->getRepeatingData()->getId() == this->getRepeatingData()->getId());

  return sp->getRepeatingDataPoints(rdi) // Get the repeating data instance's data points
    .op(RxToUnorderedMap([](std::shared_ptr<RepeatingDataPoint> dp) {return dp->getId(); })) // Index data points by (field) ID for ease of lookup
    .flat_map([fields = mFields](std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<RepeatingDataPoint>>> dpsByFieldId) {
    return fields->observe() // Iterate over all of the RepeatingData (type)'s fields
      .map([dpsByFieldId](std::shared_ptr<Field> field) {
      // Find the repeating data instance's data point for this field
      std::shared_ptr<RepeatingDataPoint> dp;
      auto found = dpsByFieldId->find(field->getId());
      if (found != dpsByFieldId->cend()) {
        dp = found->second;
      }
      // If the repeating data instance contains no data point for this field, we'll still add an entry (with an empty value)
      return std::make_shared<FieldValue>(field, dp);
      });
    });
}

rxcpp::observable<FakeVoid> RepeatingDataPuller::addMatchingInstancesTo(std::shared_ptr<StudyPuller> sp, std::shared_ptr<boost::property_tree::ptree> destination, rxcpp::observable<std::shared_ptr<RepeatingDataInstance>> candidates) {
  return candidates
    .filter([id = mRepeatingData->getId()](std::shared_ptr<RepeatingDataInstance> ri) {return ri->getRepeatingData()->getId() == id; }) // Limit to instances for this RepeatingData (type)
    .op(RxToVector()) // We need to determine if we have any repeating data instances
    .flat_map([self = SharedFrom(*this), sp, destination](std::shared_ptr<std::vector<std::shared_ptr<RepeatingDataInstance>>> instances) ->rxcpp::observable<FakeVoid> {
    // If there are no repeating data instances, don't write anything to the destination tree
    if (instances->empty()) {
      return rxcpp::observable<>::empty<FakeVoid>();
    }

    // Add repeating data instances in deterministic order so that ptrees from different runs can be compared
    std::sort(instances->begin(), instances->end(), [](std::shared_ptr<RepeatingDataInstance> lhs, std::shared_ptr<RepeatingDataInstance> rhs) {
      return lhs->getId().compare(rhs->getId()) < 0;
    });

    return RxIterate(std::move(*instances)) // Iterate over repeating data instances
      .concat_map([self, sp, destination](std::shared_ptr<RepeatingDataInstance> rdi) { // Get a ptree for each repeating data instance
      return FieldValue::Aggregate(self->getRepeatingDataInstanceFieldValues(sp, rdi));
      })
      .reduce( // Collect repeating data instance ptrees into a single ptree (which will be rendered as a JSON array)
        std::make_shared<boost::property_tree::ptree>(),
        [](std::shared_ptr<boost::property_tree::ptree> container, std::shared_ptr<boost::property_tree::ptree> instance) {
          container->push_back(std::make_pair("", *instance));
          return container;
        }
      )
      .map([destination, name = self->getRepeatingData()->getName()](std::shared_ptr<boost::property_tree::ptree> tree) { // Add the JSON array to the destination under the RepeatingData (type)'s name
        destination->push_back(std::make_pair(name, *tree));
        return FakeVoid();
      });
  });
}


rxcpp::observable<std::shared_ptr<boost::property_tree::ptree>> RepeatingDataPuller::Aggregate(
  std::shared_ptr<StudyPuller> sp,
  std::shared_ptr<std::vector<std::shared_ptr<RepeatingDataPuller>>> rdps,
  rxcpp::observable<std::shared_ptr<RepeatingDataInstance>> candidates) {
  return candidates
    .op(RxToVector()) // Ensure that candidates can be iterated over multiple times
    .flat_map([sp, rdps](std::shared_ptr<std::vector<std::shared_ptr<RepeatingDataInstance>>> rdis) {
    auto result = std::make_shared<boost::property_tree::ptree>();
    return RxIterate(*rdps)
      .flat_map([sp, rdis, result](std::shared_ptr<RepeatingDataPuller> rp) {return rp->addMatchingInstancesTo(sp, result, RxIterate(*rdis)); })
      .op(RxInstead(result));
    });
}

}
}
