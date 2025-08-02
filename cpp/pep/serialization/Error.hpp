#pragma once

#include <pep/serialization/NormalizedTypeNaming.hpp>
#include <pep/utils/SelfRegistering.hpp>
#include <pep/serialization/Serializer.hpp>

#include <cassert>
#include <functional>


namespace pep {

template <typename TDerived, bool REGISTER = true>
class DeserializableDerivedError;

// Inherit only through DeserializableDerivedError<> class (declared above; defined below)
class Error : public std::exception {
private:
  // Self registration support for derived classes
  template <class TDerived, class TRegistrar, bool REGISTER>
  friend class SelfRegistering;

  using Factory = std::function<std::exception_ptr(const std::string&)>;

  static std::unordered_map<std::string, Factory>& Factories();
  static void AddFactory(const std::string& type, Factory factory);

  template <typename T>
  static size_t RegisterType() {
    AddFactory(GetNormalizedTypeName<T>(), [](const std::string& description) {return std::make_exception_ptr(T(description)); });
    return Factories().size() - 1U;
  }

private:
  // Restrict this constructor for deserialization, for inheritance by DeserializableDerivedError<>, and for testing
  friend class Serializer<Error>;
  template <typename TDerived, bool REGISTER> friend class DeserializableDerivedError;

  Error(std::string derivedTypeName, std::string description);
  std::string mOriginalTypeName;

  bool isDeserializable() const;

public:
  explicit inline Error(const std::string& description)
    : mDescription(description) { }

  std::string mDescription;

  inline const char* what() const noexcept override { return mDescription.c_str(); }
  inline const std::string& getOriginalTypeName() const noexcept { return mOriginalTypeName; }

  static bool IsSerializable(std::exception_ptr exception) noexcept;
  static std::exception_ptr ReconstructIfDeserializable(const std::string& serialized);
  static void ThrowIfDeserializable(const std::string& serialized);
};

template <typename TDerived, bool REGISTER>
class DeserializableDerivedError : public Error, public SelfRegistering<TDerived, Error, REGISTER> {
  friend TDerived;
  inline DeserializableDerivedError(const std::string& description)
    : Error(GetNormalizedTypeName<TDerived>(), description) {
    assert(this->isDeserializable());
  }
};


}
