#pragma once

#include <cassert>
#include <map>
#include <memory>
#include <set>
#include <type_traits>

namespace pep {

// Adapted from https://stackoverflow.com/a/39132174

template <typename TItem, auto PropertyAccess> struct PropertyBasedContainer; // Just so we can specialize it

namespace detail {
/*!
  * \brief Helper struct for implementation of PropertyBasedContainer<> specializations (below).
  * \tparam TOwner The type through which a property (value) can be determined.
  */
template <typename TOwner>
struct PropertyOwner {
  static const TOwner& Reference(const TOwner& owner) { return owner; } //NOLINT(bugprone-return-const-ref-from-parameter)
  static const TOwner& Dereference(const TOwner* const& owner) { assert(owner != nullptr); return *owner; }
  static const TOwner& DereferenceShared(const std::shared_ptr<TOwner>& owner) { return Dereference(owner.get()); }
  static const TOwner& DereferenceUnique(const std::unique_ptr<TOwner>& owner) { return Dereference(owner.get()); }

  /*!
  * \brief Converts a pointer to member variable ("field") to a free function.
  * \tparam TProperty The field type.
  * \param owner An instance of the type containing the field.
  * \return (A const reference to) the field's value.
  */
  template <typename TProperty, TProperty(TOwner::* Field)>
  static const TProperty& GetFieldProperty(const TOwner& owner) { return owner.*Field; }

  /*!
  * \brief Converts a pointer to const member function ("method") to a free function.
  * \tparam TProperty The return type of the method.
  * \param owner An instance of the type containing the method.
  * \return The return value of invoking the method on the owner object.
  */
  template <typename TProperty, TProperty(TOwner::* Method)() const>
  static TProperty GetMethodProperty(const TOwner& owner) { return (owner.*Method)(); }
};

/*!
  * \brief Compare struct for PropertyBasedContainer<> containers.
  * \tparam TItem The type of item stored in the container.
  * \tparam TProperty The return type of the GetPropertyValue function.
  * \tparam TOwner The type through which the property (value) can be determined.
  * \tparam GetOwnerReference A function that converts (e.g. dereferences) a container item to a property owner instance.
  * \tparam GetPropertyValue A function that produces the property (value).
  */
template <typename TItem, typename TProperty, typename TOwner, const TOwner& (*GetOwnerReference)(const TItem&), TProperty(*GetPropertyValue)(const TOwner&)>
struct PropertyBasedCompare {
  using Property = std::remove_const_t<std::remove_reference_t<TProperty>>;

  bool operator()(const TItem& lhs, const TItem& rhs) const { return GetPropertyValue(GetOwnerReference(lhs)) < GetPropertyValue(GetOwnerReference(rhs)); }
  bool operator()(const TItem& lhs, const Property& rhs) const { return GetPropertyValue(GetOwnerReference(lhs)) < rhs; }
  bool operator()(const Property& lhs, const TItem& rhs) const { return lhs < GetPropertyValue(GetOwnerReference(rhs)); }

  using is_transparent = void;
};

}

/*!
  * \brief Provides container types keyed on items with unique property values. This specialization requires a (free) function that produces the item property.
  * \tparam TItem The type of item stored in the container
  * \tparam TProperty The return type of the GetPropertyFunction.
  * \tparam GetPropertyFunction A function producing an item's property value.
  * \remark E.g. to keep a set of shared_ptrs to User instances with unique IDs:
  *     struct User { int id; }
  *     int GetUserId(const User& User) { return user.id; }
  *     PropertyBasedContainer<std::shared_ptr<User>, &GetUserId>::set myUsers;
  */
template <typename TItem, typename TProperty, TProperty(*GetPropertyFunction)(const TItem&)>
struct PropertyBasedContainer<TItem, GetPropertyFunction> {
  using Compare = detail::PropertyBasedCompare<TItem, TProperty, TItem,
    &detail::PropertyOwner<TItem>::Reference,
    GetPropertyFunction>;

  using set = std::set<TItem, Compare>;

  template <typename TValue>
  using map = std::map<TItem, TValue, Compare>;
};

/*!
  * \brief Provides container types keyed on pointers to items with unique property values. This specialization requires a (free) function that produces the item property.
  * \tparam TItem The type of item stored in the container
  * \tparam TProperty The return type of the GetPropertyFunction.
  * \tparam GetPropertyFunction A function producing an item's property value.
  * \remark E.g. to keep a set of pointers to User instances with unique IDs:
  *     struct User { int id; }
  *     int GetUserId(const User& user) { return user.id; }
  *     PropertyBasedContainer<User*, &GetUserId>::set myUsers;
  * \remark We "remove_const_t" and then re-add "const" to get prevent compilation failure when TItem itself is const, e.g. when
  *         we compile a UniqueItemsContainer<const std::string*>. While I don't fully understand why this would not compile,
  *         the "const remove_const_t<>" approach gets rid of the problem. (I suspect that the compiler selects a different
  *         specialization due to SFINAE-related subtleties, but we want *this* specialization to be used whenever we need a
  *         PropertyBasedContainer<> with a pointer as a key.
  */
template <typename TItem, typename TProperty, TProperty(*GetPropertyFunction)(const std::remove_const_t<TItem>&)>
struct PropertyBasedContainer<TItem*, GetPropertyFunction> {
  using Compare = detail::PropertyBasedCompare<TItem*, TProperty, TItem,
    &detail::PropertyOwner<TItem>::Dereference,
    GetPropertyFunction>;

  using set = std::set<TItem*, Compare>;

  template <typename TValue>
  using map = std::map<TItem*, TValue, Compare>;
};

/*!
  * \brief Provides container types keyed on items with unique property values. This specialization requires a const method that produces the item property.
  * \tparam TItem The type of item stored in the container
  * \tparam TProperty The property type used as the container's key.
  * \tparam GetPropertyMethod A const method producing an item's property value.
  * \remark E.g. to keep a set of User instances with unique IDs:
  *     struct User { int getId() const; }
  *     PropertyBasedContainer<User, &User::getId>::set myUsers;
  */
template <typename TItem, typename TProperty, TProperty(TItem::*GetPropertyMethod)() const>
struct PropertyBasedContainer<TItem, GetPropertyMethod> {
  using Compare = detail::PropertyBasedCompare<TItem, TProperty, TItem,
    &detail::PropertyOwner<TItem>::Reference,
    &detail::PropertyOwner<TItem>::template GetMethodProperty<TProperty, GetPropertyMethod>>;

  using set = std::set<TItem, Compare>;

  template <typename TValue>
  using map = std::map<TItem, TValue, Compare>;
};

/*!
  * \brief Provides container types keyed on shared_ptrs to items with unique property values. This specialization requires a const method that produces the item property.
  * \tparam TOwner The type of item stored in shared_ptrs in the container.
  * \tparam TProperty The property type used as the container's key.
  * \tparam GetPropertyMethod A const method producing an item's property value.
  * \remark E.g. to keep a set of shared_ptrs to User instances with unique IDs:
  *     struct User { int getId() const; }
  *     PropertyBasedContainer<std::shared_ptr<User>, &User::getId>::set myUsers;
  */
template <typename TOwner, typename TProperty, TProperty(TOwner::*GetPropertyMethod)() const>
struct PropertyBasedContainer<std::shared_ptr<TOwner>, GetPropertyMethod> {
  using Compare = detail::PropertyBasedCompare<std::shared_ptr<TOwner>, TProperty, TOwner,
    &detail::PropertyOwner<TOwner>::DereferenceShared,
    &detail::PropertyOwner<TOwner>::template GetMethodProperty<TProperty, GetPropertyMethod>>;

  using set = std::set<std::shared_ptr<TOwner>, Compare>;

  template <typename TValue>
  using map = std::map<std::shared_ptr<TOwner>, TValue, Compare>;
};

/*!
  * \brief Provides container types keyed on unique_ptrs to items with unique property values. This specialization requires a const method that produces the item property.
  * \tparam TOwner The type of item stored in unique_ptrs in the container.
  * \tparam TProperty The property type used as the container's key.
  * \tparam GetPropertyMethod A const method producing an item's property value.
  * \remark E.g. to keep a set of unique_ptrs to User instances with unique IDs:
  *     struct User { int getId() const; }
  *     PropertyBasedContainer<std::unique_ptr<User>, &User::getId>::set myUsers;
  */
template <typename TOwner, typename TProperty, TProperty(TOwner::* GetPropertyMethod)() const>
struct PropertyBasedContainer<std::unique_ptr<TOwner>, GetPropertyMethod> {
  using Compare = detail::PropertyBasedCompare<std::unique_ptr<TOwner>, TProperty, TOwner,
    &detail::PropertyOwner<TOwner>::DereferenceUnique,
    &detail::PropertyOwner<TOwner>::template GetMethodProperty<TProperty, GetPropertyMethod>>;

  using set = std::set<std::unique_ptr<TOwner>, Compare>;

  template <typename TValue>
  using map = std::map<std::unique_ptr<TOwner>, TValue, Compare>;
};

//XXX GCC bug: noexcept function should be converted to non-noexcept
#if defined(__GNUC__) && !defined(__clang__)
template <typename TItem, typename TProperty, TProperty(TItem::*GetPropertyMethod)() const noexcept>
struct PropertyBasedContainer<TItem, GetPropertyMethod>
  : PropertyBasedContainer<TItem, static_cast<TProperty(TItem::*)() const>(GetPropertyMethod)> {};

template <typename TOwner, typename TProperty, TProperty(TOwner::*GetPropertyMethod)() const noexcept>
struct PropertyBasedContainer<std::shared_ptr<TOwner>, GetPropertyMethod>
  : PropertyBasedContainer<std::shared_ptr<TOwner>, static_cast<TProperty(TOwner::*)() const>(GetPropertyMethod)> {};

template <typename TOwner, typename TProperty, TProperty(TOwner::* GetPropertyMethod)() const noexcept>
struct PropertyBasedContainer<std::unique_ptr<TOwner>, GetPropertyMethod>
  : PropertyBasedContainer<std::unique_ptr<TOwner>, static_cast<TProperty(TOwner::*)() const>(GetPropertyMethod)> {};
#endif

/*!
  * \brief Provides container types keyed on items with unique property values. This specialization requires a member variable ("field") containing the item property.
  * \tparam TItem The type of item stored in the container
  * \tparam TProperty The property type used as the container's key.
  * \tparam Field The member variable containing an item's property value.
  * \remark E.g. to keep a set of User instances with unique IDs:
  *     struct User { int id; }
  *     PropertyBasedContainer<User, &User::id>::set myUsers;
  */
template <typename TItem, typename TProperty, TProperty (TItem::*Field)>
struct PropertyBasedContainer<TItem, Field> {
  using Compare = detail::PropertyBasedCompare<TItem, const TProperty&, TItem,
    &detail::PropertyOwner<TItem>::Reference,
    &detail::PropertyOwner<TItem>::template GetFieldProperty<TProperty, Field>>;

  using set = std::set<TItem, Compare>;

  template <typename TValue>
  using map = std::map<TItem, TValue, Compare>;
};


template <typename TItem> struct UniqueItemsContainer; // No implementation needed for this (general) case: people will just use std::set<> or std::map<> directly.

/*!
  * \brief Provides container types keyed on pointers to unique items.
  * \tparam TItem The type of item stored in the container
  * \remark E.g. to keep a set of pointers to unique strings:
  *     UniqueItemsContainer<std::string*>::set myStrings;
  */
template <typename TItem>
struct UniqueItemsContainer<TItem*> : public PropertyBasedContainer<TItem*, &detail::PropertyOwner<TItem>::Reference> {
};

}
