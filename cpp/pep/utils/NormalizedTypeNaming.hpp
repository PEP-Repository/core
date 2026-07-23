#pragma once

#include <pep/utils/PlainTypeName.hpp>
#include <memory>

namespace pep {

/*!
* \brief Base class for NormalizedTypeNamer<>. Add non-template methods here to prevent template-induced code bloat.
*/
class BasicNormalizedTypeNamer {
  template <typename T>
  friend struct NormalizedTypeNamer;

private:
  static std::string GetTypeName(const std::string& plain);
};

template <typename T>
struct NormalizedTypeNamer : public BasicNormalizedTypeNamer {
  static inline std::string GetTypeName() { return BasicNormalizedTypeNamer::GetTypeName(GetPlainTypeName<T>()); }
};

template <typename T>
std::string GetNormalizedTypeName() {
  return NormalizedTypeNamer<T>::GetTypeName();
}

template <typename T>
struct NormalizedTypeNamer<std::shared_ptr<T>> : public BasicNormalizedTypeNamer {
  static inline std::string GetTypeName() { return "Shared" + GetNormalizedTypeName<T>(); }
};

}
