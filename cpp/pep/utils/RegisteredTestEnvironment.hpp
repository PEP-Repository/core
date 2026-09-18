#pragma once

#include <gtest/gtest.h>
#include <pep/utils/SelfRegistering.hpp>
#include <functional>

namespace pep {

class RegisteredTestEnvironment : public ::testing::Environment {
  template <class TDerived, class TRegistrar, bool registerDerived>
  friend class SelfRegistering;

private:
  using Factory = std::function<RegisteredTestEnvironment* (std::span<const char* const> args)>;

  static std::optional<Factory>& RegisteredFactory();
  static void RegisterFactory(const Factory& factory);

  template <typename T>
  static bool RegisterType() {
    static_assert(std::is_base_of<RegisteredTestEnvironment, T>::value, "Only invoke this method with types that inherit from RegisteredTestEnvironment");
    RegisterFactory([](std::span<const char* const> args) {return new T(args); });
    return true;
  }

protected:
  RegisteredTestEnvironment(std::span<const char* const> /*args*/) noexcept {}

public:
  /// \brief Creates a test environment if a type has been registered.
  /// \param args The command line arguments
  /// \return A pointer to a newly create test environment instance, or nullptr if no type has been registered.
  static RegisteredTestEnvironment* Create(std::span<const char* const> args);
};


/// \brief Base class for (process-wide) test environments that should be set up for a test executable. ALSO SEE THE WARNING associated with this class.
/// \tparam TDerived The (derived) class that implements the ::testing::Environment.
/// \warning To ensure that the (derived) class is found by the test executable, its definition should be sourced **directly** into the test executable,
///          i.e. linking the executable to a library that defines the type won't work. See https://gitlab.pep.cs.ru.nl/pep/core/-/work_items/2980#note_63709.
template <typename TDerived>
class SelfRegisteringTestEnvironment : public pep::SelfRegistering<TDerived, RegisteredTestEnvironment>, public RegisteredTestEnvironment {
protected:
  SelfRegisteringTestEnvironment(std::span<const char* const> args) noexcept
    : RegisteredTestEnvironment(args) {
  }
};

}

