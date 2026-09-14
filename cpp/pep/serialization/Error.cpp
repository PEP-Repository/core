#include <pep/serialization/Error.hpp>
#include <pep/serialization/ErrorSerializer.hpp>
#include <pep/serialization/Serialization.hpp>

namespace pep {

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
      return std::make_exception_ptr(std::move(deserialized));
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
