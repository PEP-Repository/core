#pragma once

#include <pep/utils/BuildFlavor.hpp>

#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>

#include <pep/castor/CastorConnection.hpp>
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

#if BUILD_HAS_DEBUG_FLAVOR()
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
  CastorObject(JsonPtr json, const std::string& idField = DEFAULT_ID_FIELD);

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
  friend TChild;
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
