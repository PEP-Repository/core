#pragma once

#include <type_traits>

namespace pep {

// See bottom of this source for sample code.

/*!
 * \brief A mixin class that invokes a registration function at program initialization time,
 *        allowing the registrar to know about all self-registering types without needing to
 *        change code when a registering type is added, removed, or changed.
 * \tparam TDerived The (derived) class to register.
 * \tparam TRegistrar The registrar class. Must provide a "template <typename T> static some_type RegisterType()",
 *               which is invoked once for every TDerived type (during static variable initialization).
 * \tparam REGISTER Development helper: specify "false" to (temporarily) disable self-registration for specific derived types.
 * \remark See e.g. https://stackoverflow.com/a/10333643.
 */
template <class TDerived, class TRegistrar, bool REGISTER = true>
class SelfRegistering;

/*!
 * \brief The actual implementation: (a specialization that) registers the TDerived class with its TRegistrar class.
 */
template <class TDerived, class TRegistrar>
class SelfRegistering<TDerived, TRegistrar, true> {
  using RegistrationId = decltype(TRegistrar::template RegisterType<TDerived>());

protected:
  static const RegistrationId REGISTRATION_ID;
  SelfRegistering() = default;
  SelfRegistering(const SelfRegistering&) = default;

public:
  virtual ~SelfRegistering() noexcept {
    static_assert(std::is_base_of_v<SelfRegistering, TDerived>, "The class specified as TDerived must inherit from this class");
    // Reference the static const to force its initialization, causing TRegistrar::RegisterType<TDerived>() to be invoked
    (void)REGISTRATION_ID;
  }
};

template <class TDerived, class TRegistrar>
const typename SelfRegistering<TDerived, TRegistrar, true>::RegistrationId SelfRegistering<TDerived, TRegistrar, true>::REGISTRATION_ID
= TRegistrar::template RegisterType<TDerived>();

/*!
 * \brief Development helper specialization that doesn't actually register the derived class.
 * \remark Usage: to (temporarily) disable self-registration, change
 *          class MyClass : public SelfRegistering<MyClass, MyBase>
 *        to
 *          class MyClass : public SelfRegistering<MyClass, MyBase, false>
 */
template <class TDerived, class TRegistrar>
class SelfRegistering<TDerived, TRegistrar, false> {
protected:
  SelfRegistering() = default;
  SelfRegistering(const SelfRegistering&) = default;
public:
  virtual ~SelfRegistering() noexcept {
    static_assert(std::is_base_of_v<SelfRegistering, TDerived>, "The class specified as TDerived must inherit from this class");
  }
};


/* Sample code:
 *
 * // MyBase.hpp
 * #include <pep/utils/SelfRegistering.hpp>
 *
 * class MyBase {
 * private:
 *   template <class TDerived, class TBase, bool REGISTER>
 *   friend class SelfRegistering;
 *
 *   // Helper function to work around the static initialization order fiasco: ensures that our static variable
 *   // is available before other (namespace-scoped) static instances are initialized.
 *   static std::vector<std::shared_ptr<MyBase>>& GetDerivedInstances() { // Note: returns by reference
 *     static std::vector<std::shared_ptr<MyBase>> instances;
 *     return instances;
 *   }
 *
 *   template <class TDerived>
 *   static size_t RegisterType() {
 *     auto& instances = GetDerivedInstances(); // Note: by reference
 *     auto result = instances.size();
 *     instances.push_back(std::make_shared<TDerived>());
 *     return result;
 *   }
 *
 * public:
 *   virtual const char* name() const noexcept = 0;
 *
 *   // Produces derived class instances without changing code when an additional derived class is added.
 *   inline std::vector<std::shared_ptr<MyBase>> GetAll() { // Return by value: we don't want callers messing with our vector
 *     return GetDerivedInstances();
 *   }
 * };
 *
 *
 * // ********************************************************************************
 * // FirstDerived.cpp (no associated header needed)
 * #include <MyBase.hpp>
 *
 * class FirstDerived : public SelfRegistering<FirstDerived, MyBase>, public MyBase {
 *   const char* name() const noexcept override { return "First"; }
 * };
 *
 *
 * // ********************************************************************************
 * // SecondDerived.cpp (no associated header needed)
 * #include <MyBase.hpp>
 *
 * class SecondDerived : public SelfRegistering<SecondDerived, MyBase>, public MyBase {
 *   const char* name() const noexcept override { return "Second"; }
 * };
 *
 *
 * // ********************************************************************************
 * // Program.cpp
 * #include <MyBase.hpp>
 * #include <iostream>
 *
 * int main() {
 *   for (auto instance: MyBase::GetAll()) {   // Only knows about base class, but...
 *     std::cout << instance->name() << '\n';  // ...invokes a method on all derived types.
 *   }
 *   return 0;
 * }
 */

}
