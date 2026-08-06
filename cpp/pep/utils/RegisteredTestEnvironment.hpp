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

template <typename TDerived>
class SelfRegisteringTestEnvironment : public pep::SelfRegistering<TDerived, RegisteredTestEnvironment>, public RegisteredTestEnvironment {
protected:
  SelfRegisteringTestEnvironment(std::span<const char* const> args) noexcept
    : RegisteredTestEnvironment(args) {
  }
};

}

