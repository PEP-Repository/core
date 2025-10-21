#include <pep/serialization/NormalizedTypeNaming.hpp>
#include <cassert>

namespace pep {

std::string BasicNormalizedTypeNamer::GetTypeName(const std::string& plain) {
  std::string name = plain;

  // Backward compatible: "normalized" means "no template brackets"
  // This a.o. makes the name (more) portable to other languages
  if (name.find_first_of('<') != std::string::npos) {
    throw std::runtime_error("Normalized type name cannot contain template brackets. Please (partially) specialize NormalizedTypeNamer<> for this template class: " + name);
  }
  assert(name.find_first_of('>') == std::string::npos);

  // Remove any namespaces
  auto index = name.find_last_of(':');
  if (index != std::string::npos) {
    name = name.substr(index + 1);
  }

  return name;
}

}
