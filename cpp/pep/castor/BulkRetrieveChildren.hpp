#pragma once

#include <pep/castor/CastorObject.hpp>
#include <boost/property_tree/ptree.hpp>

namespace pep {
namespace castor {

/*!
 * \brief Retrieve a list of objects that are children of specified parent objects.
 *
 * \tparam ChildType The type of the objects in the list
 * \tparam ParentType The type of the parent object (which is passed to every child item's constructor)
 * \param parentsById a map associating parent IDs with parent object instances
 * \param apiPath path to request from the API
 * \param embeddedItemsNodeName name of the node under the "_embedded" node that contains data on child items
 * \param parentIdNodeName name of the node within the child data that specifies the parent ID
 * \return Observable that, if no error occurs, emits one ChildType for every item in the list
 */
template <class ChildType, class ParentType>
rxcpp::observable<std::shared_ptr<ChildType>> BulkRetrieveChildren(
  std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<ParentType>>> parentsById,
  const std::string& apiPath, const std::string& embeddedItemsNodeName, const std::string& parentIdNodeName) {
  static_assert(std::is_base_of<CastorObject, ParentType>::value, "ParentType must derive from (or be) CastorObject");

  // Don't access the API at all if there are no parents to assign child instances to
  if (parentsById->empty()) {
    return rxcpp::observable<>::empty<std::shared_ptr<ChildType>>();
  }

  // Get the (first parent's) CastorConnection instance so we can send requests over it
  auto connection = parentsById->cbegin()->second->getConnection();
  // All parent instances must/should belong to the same CastorConnection
  assert(std::all_of(parentsById->cbegin(), parentsById->cend(), [connection](const auto& entry) {return entry.second->getConnection() == connection; }));

  return connection->getJsonEntries(apiPath, embeddedItemsNodeName) // Retrieve JSON entries for children
    .map([parentsById, parentIdNodeName](JsonPtr json) -> std::shared_ptr<ChildType> {
    auto id = json->get<std::string>(parentIdNodeName); // Get the parent ID from the child JSON
    auto position = parentsById->find(id); // Find the parent with the ID that the child specifies
    if (position == parentsById->cend()) { // No parent with the specified ID could be found: e.g. a SurveyDataPoint for a SurveyInstance whose Survey has been removed from the SurveyPackage
      return nullptr; // Return sentinel value that's filtered out (below)
    }
    return ChildType::Create(position->second, json); // Found the parent instance: use it to create the child instance
      })
    .filter([](std::shared_ptr<ChildType> child) {return child != nullptr; }); // Filter out sentinel values for child instances that couldn't be created
}

/*!
 * \brief Retrieve a list of objects that are children of specified parent objects.
 *
 * \tparam ChildType The type of the objects in the list
 * \tparam ParentType The type of the parent object (which is passed to every child item's constructor)
 * \param parents the parent object instances
 * \param apiPath path to request from the API
 * \param embeddedItemsNodeName name of the node under the "_embedded" node that contains data on child items
 * \param parentIdNodeName name of the node within the child data that specifies the parent ID
 * \return Observable that, if no error occurs, emits one ChildType for every item in the list
 */
template <class ChildType, class ParentType>
rxcpp::observable<std::shared_ptr<ChildType>> BulkRetrieveChildren(
  rxcpp::observable<std::shared_ptr<ParentType>> parents,
  const std::string& apiPath, const std::string& embeddedItemsNodeName, const std::string& parentIdNodeName) {
  static_assert(std::is_base_of<CastorObject, ParentType>::value, "ParentType must derive from (or be) CastorObject");

  return parents
    .op(RxToUnorderedMap([](std::shared_ptr<ParentType> parent) {return parent->getId(); }))
    .flat_map([apiPath, embeddedItemsNodeName, parentIdNodeName](std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<ParentType>>> parentsById) {
    return BulkRetrieveChildren<ChildType, ParentType>(parentsById, apiPath, embeddedItemsNodeName, parentIdNodeName);
      });
}

} // namespace castor
} // namespace pep
