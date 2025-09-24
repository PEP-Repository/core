#pragma once

#include <gtest/gtest.h>
#include <pep/utils/SelfRegistering.hpp>
#include <functional>

namespace pep {

class RegisteredTestEnvironment : public ::testing::Environment {
  template <class TDerived, class TRegistrar, bool REGISTER>
  friend class SelfRegistering;

private:
  using Factory = std::function<RegisteredTestEnvironment* (int, char**)>;
  
  static std::optional<Factory>& RegisteredFactory();
  static void RegisterFactory(const Factory& factory);

  template <typename T>
  static bool RegisterType() {
    static_assert(std::is_base_of<RegisteredTestEnvironment, T>::value, "Only invoke this method with types that inherit from RegisteredTestEnvironment");
    RegisterFactory([](int argc, char** argv) {return new T(argc, argv); });
    return true;
  }

protected:
  RegisteredTestEnvironment(int, char**) noexcept {}

public:
  /*\brief Creates a test environment if a type has been registered.
  * \param argc The number of command line arguments
  * \param argv The values of command line arguments
  * \return A pointer to a newly create test environment instance, or nullptr if no type has been registered.
  */
  static RegisteredTestEnvironment* Create(int argc, char* argv[]);
};

template <typename TDerived>
class SelfRegisteringTestEnvironment : public pep::SelfRegistering<TDerived, RegisteredTestEnvironment>, public RegisteredTestEnvironment {
protected:
  SelfRegisteringTestEnvironment(int argc, char** argv) noexcept
    : RegisteredTestEnvironment(argc, argv) {
  }
};

}

