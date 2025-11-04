#include <pep/serialization/Error.hpp>
#include <pep/serialization/ErrorSerializer.hpp>
#include <pep/serialization/Serialization.hpp>

namespace pep {

std::unordered_map<std::string, Error::Factory>& Error::Factories() {
  static std::unordered_map<std::string, Error::Factory> result;
  return result;
}

void Error::AddFactory(const std::string& type, Factory factory) {
  auto emplaced = Factories().try_emplace(type, std::move(factory)).second;
  if (!emplaced) {
    throw std::runtime_error("Could not register a second factory for error type " + type);
  }
}

Error::Error(std::string derivedTypeName, std::string description)
  : mOriginalTypeName(std::move(derivedTypeName)), mDescription(std::move(description)) {
  assert(mOriginalTypeName != GetNormalizedTypeName<Error>()); // Leave mOriginalTypeName empty for basic Error instances
  assert(this->isDeserializable()); // Do not test-and-throw: allow clients to raise a basic Error instance when receiving an unsupported derived type
}

bool Error::isDeserializable() const {
  if (mOriginalTypeName.empty()) {
    return true;
  }
  auto registered = Factories();
  return registered.find(mOriginalTypeName) != registered.cend();
}

bool Error::IsSerializable(std::exception_ptr exception) noexcept {
  if (exception == nullptr) {
    return false;
  }

  try {
    std::rethrow_exception(exception);
  }
  catch (const Error&) {
    return true;
  }
  catch (...) {
    return false;
  }
}

std::exception_ptr Error::ReconstructIfDeserializable(std::string_view serialized) {
  if (serialized.size() >= sizeof(MessageMagic)) {
    if (GetMessageMagic(serialized) == MessageMagician<Error>::GetMagic()) { // It's deserializable
      // Deserialize properties into base class instance
      Error deserialized = Serialization::FromString<Error>(serialized);

      // If it was originally a different (derived) type, try to reconstruct that
      auto type = deserialized.mOriginalTypeName;
      if (!type.empty()) {
        // Find a factory function for this original type name
        auto& factories = Factories();
        auto found = factories.find(type);
        if (found != factories.cend()) {
          // Invoke factory to create an exception_ptr for this original type name
          return found->second(deserialized.mDescription);
        }

        // An original type name was specified but we don't have a factory for it. Presumably our software is outdated. Issue a warning...
        LOG("Network error handling", error)
          << "Errors of derived " + type << " type cannot be transported across the network. "
          << "Please ensure that the derived type is properly registered. You may need to upgrade your software.";
        // ... then register a (degenerate) factory so the warning is only issued once...
        AddFactory(deserialized.mOriginalTypeName, [type](const std::string& description) {
          return std::make_exception_ptr(Error(type, description));
        });
        // ... and finally let default handling return the basic Error instance that we deserialized.
      }

      // Cannot or should not create derived instance: return deserialized Error instance
      return std::make_exception_ptr(deserialized);
    }
  }
  return nullptr;
}

void Error::ThrowIfDeserializable(std::string_view serialized) {
  auto ptr = Error::ReconstructIfDeserializable(serialized);
  if (ptr != nullptr) {
    std::rethrow_exception(ptr);
  }
}

}
