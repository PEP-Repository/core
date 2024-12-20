#pragma once

#include <pep/utils/BuildFlavor.hpp>

#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>

#include <pep/castor/CastorConnection.hpp>
#include <pep/castor/Ptree.hpp>
#include <pep/utils/Shared.hpp>
#include <pep/async/RxUtils.hpp>

namespace pep {
namespace castor {

//! Base class for different objects that can be retrieved from the Castor API
 class CastorObject : public std::enable_shared_from_this<CastorObject> {
 public:
  virtual ~CastorObject() = default;
  
  //! \return The CastorConnection for this object
  virtual std::shared_ptr<CastorConnection> getConnection() const = 0;

  //! \return A url that can be used to retrieve this object from the Castor API
  virtual std::string makeUrl() const = 0;

  //! \return The id for this object. This is a UUID for use within the API itself
  std::string getId() const;

 private:
  std::string mId;

#if BUILD_HAS_DEBUG_FLAVOR
  std::string mJson;

 public:
  /*!
   * \return The pretty printed Json representation for this object. For debugging purposes.
   */
  std::string toJsonString() const { return mJson; }

#endif

 protected:
  static const std::string DEFAULT_ID_FIELD;
  /*!
   * \brief Construct a new CastorObject
   *
   * \param json The %Json response from the Castor API for this object
   * \param idField The name of the field in which the object's ID is stored. Defaults to "id"
   */
  CastorObject(JsonPtr json, const std::string& idField = DEFAULT_ID_FIELD)
     : mId(GetFromPtree<std::string>(*json, idField)) {
  }

  /*!
   * \brief Retrieve a list of objects that are children of a specified parent object.
   *
   * \tparam ChildType The type of the objects in the list
   * \tparam ParentType The type of the parent object (which is passed to every child item's constructor)
   * \param parent the parent object instance
   * \param apiPath path to request from the API
   * \param embeddedItemsNodeName name of the node under the "_embedded" node that contains data on child items
   * \return Observable that, if no error occurs, emits one ChildType for every item in the list
   */
  template<class ChildType, class ParentType>
  static rxcpp::observable<std::shared_ptr<ChildType>> RetrieveList(std::shared_ptr<ParentType> parent, const std::string& apiPath, const std::string& embeddedItemsNodeName) {
    static_assert(std::is_base_of<CastorObject, ParentType>::value, "ParentType must derive from (or be) CastorObject");

    return parent->getConnection()->getJsonEntries(apiPath, embeddedItemsNodeName)
      .map([parent](JsonPtr childProperties) {
      return ChildType::Create(parent, childProperties);
        });
  }

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
  static rxcpp::observable<std::shared_ptr<ChildType>> BulkRetrieveList(
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
  static rxcpp::observable<std::shared_ptr<ChildType>> BulkRetrieveList(
    rxcpp::observable<std::shared_ptr<ParentType>> parents,
    const std::string& apiPath, const std::string& embeddedItemsNodeName, const std::string& parentIdNodeName) {
    static_assert(std::is_base_of<CastorObject, ParentType>::value, "ParentType must derive from (or be) CastorObject");

    return parents
      .op(RxToUnorderedMap([](std::shared_ptr<ParentType> parent) {return parent->getId(); }))
      .flat_map([apiPath, embeddedItemsNodeName, parentIdNodeName](std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<ParentType>>> parentsById) {
      return BulkRetrieveList<ChildType, ParentType>(parentsById, apiPath, embeddedItemsNodeName, parentIdNodeName);
        });
  }
};

/*!
 * \brief Utility base class for classes that have a parent (in our object model).
 * 
 * \tparam TParent The parent type.
 * \tparam TBase The class from which this class should inherit. This type must be CastorObject or derive from it.
 */
template <typename TParent, typename TBase = CastorObject>
class ParentedCastorObject : public TBase {
private:
  std::shared_ptr<TParent> mParent;

protected:
  ParentedCastorObject(std::shared_ptr<TParent> parent, JsonPtr json)
    : TBase(json), mParent(parent) {
    assert(mParent != nullptr);
  }

  ParentedCastorObject(std::shared_ptr<TParent> parent, JsonPtr json, const std::string& idField)
    : TBase(json, idField), mParent(parent) {
    assert(mParent != nullptr);
  }

  std::shared_ptr<TParent> getParent() const { return mParent; }

public:
  ~ParentedCastorObject() override {
    static_assert(std::is_base_of<CastorObject, TParent>::value, "Parent type must inherit from CastorObject");
    static_assert(std::is_base_of<CastorObject, TBase>::value, "Base type must inherit from (or be) CastorObject");
  }

  //! \return The CastorConnection for this object
  std::shared_ptr<CastorConnection> getConnection() const override { return mParent->getConnection(); }

protected:
  static std::string GetParentRelativeEndpoint(std::shared_ptr<TParent> parent, const std::string& relative) {
    return parent->makeUrl() + "/" + relative;
  }

  std::string makeParentRelativeUrl(const std::string& relative) const { return GetParentRelativeEndpoint(mParent, relative) + "/" + this->getId(); }
};

/*!
 * \brief Utility base class for classes that follow the most common pattern for Castor API objects and locations:
 *        - they have a parent, and
 *        - all children belonging to the parent can be listed by suffixing a fixed string to the parent's URL, and
 *        - each child's own URL is equal to the (parent-relative) list URL plus the child's (own) ID.
 *
 * \tparam TChild The (most) derived type that inherits this class.
 * \tparam TParent The parent type.
 *
 * \remark Inheritors must define static const strings RELATIVE_API_ENDPOINT and EMBEDDED_API_NODE_NAME in their own (TChild) type.
 */
template <typename TChild, typename TParent>
class SimpleCastorChildObject : public ParentedCastorObject<TParent> {
protected:
  SimpleCastorChildObject(std::shared_ptr<TParent> parent, JsonPtr json)
    : ParentedCastorObject<TParent>(parent, json) {
  }

public:
  //! \return A url that can be used to retrieve this child object from the Castor API
  std::string makeUrl() const override { return this->makeParentRelativeUrl(TChild::RELATIVE_API_ENDPOINT); }

  /*!
 * \brief Get a list of objects that are children of a specified parent object
 *
 * \param parent the parent object instance
 * \return Observable that, if no error occurs, emits one ChildType for every item in the list
 */
  static rxcpp::observable<std::shared_ptr<TChild>> RetrieveForParent(std::shared_ptr<TParent> parent) {
    return CastorObject::RetrieveList<TChild, TParent>(
      parent,
      ParentedCastorObject<TParent>::GetParentRelativeEndpoint(parent, TChild::RELATIVE_API_ENDPOINT),
      TChild::EMBEDDED_API_NODE_NAME);
  }
};

}
}
