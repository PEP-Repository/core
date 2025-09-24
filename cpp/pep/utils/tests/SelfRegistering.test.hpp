#pragma once

#include <pep/utils/SelfRegistering.hpp>
#include <boost/type_index.hpp>
#include <algorithm>
#include <vector>

namespace pep::tests {

class TestRegistrar;


template <typename TDerived>
class TestableSelfRegistering : public pep::SelfRegistering<TDerived, TestRegistrar> {
protected:
  TestableSelfRegistering(const char* constructorFile) noexcept
    : mConstructorFile(constructorFile) {
  }

public:
  const char* getConstructorFile() const noexcept { return mConstructorFile; }

private:
  const char* mConstructorFile;
};


class TestRegistrar {
  template <class TDerived, class TRegistrar, bool REGISTER>
  friend class pep::SelfRegistering;

public:
  struct RegisteredTraits {
    std::string prettyName;
    std::string constructorFile;
  };

private:
  // Defining our static in a function ensures that it's available when SelfRegistering<> initializes its (also static) constants.
  // Google "static initialization fiasco" to learn more.
  static std::vector<RegisteredTraits>& RegisteredTypeTraits() noexcept { // Note: returns by reference
    static std::vector<RegisteredTraits> result;
    return result;
  }

  // Invoked by SelfRegistering<>
  template <typename T>
  static size_t RegisterType() {
    static_assert(std::is_base_of<TestableSelfRegistering<T>, T>::value, "Can only register types that inherit from TestableSelfRegistering<>");

    auto prettyName = boost::typeindex::type_id<T>().pretty_name();
    if (KnowsType(prettyName)) {
      throw std::runtime_error("Can't register type name " + prettyName + " multiple times");
    }

    RegisteredTraits traits{
      prettyName,
      T().getConstructorFile()
    };
    RegisteredTypeTraits().push_back(traits);

    return RegisteredTypeTraits().size() - 1U;
  }

public:
  // Public function returns a _const_ reference to prevent anyone from messing with our vector-of-registered-types.
  static const std::vector<RegisteredTraits>& GetRegisteredTypeTraits() noexcept {
    return RegisteredTypeTraits();
  }

  static bool KnowsType(const std::string& typeName) {
    const auto& traits = RegisteredTypeTraits();
    auto end = traits.cend();
    auto position = std::find_if(traits.cbegin(), traits.cend(), [typeName](const RegisteredTraits& instance) {
      // Match as substring: caller will (likely) specify just class name "Xyz" while the "pretty name" contains decorations, e.g. "class ns::sub::Xyz"
      return instance.prettyName.find(typeName) != std::string::npos;
      });
    return position != end;
  }
};

}
