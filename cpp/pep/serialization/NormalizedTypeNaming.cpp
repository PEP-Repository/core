#include <pep/serialization/NormalizedTypeNaming.hpp>
#include <cassert>

namespace pep {

std::string BasicNormalizedTypeNamer::GetTypeName(const std::string& plain) {
  // Backward compatible: "normalized" means "no template brackets"
  // This a.o. makes the name (more) portable to other languages
  if (plain.find_first_of('<') != std::string::npos) {
    throw std::runtime_error("Normalized type name cannot contain template brackets. Please (partially) specialize NormalizedTypeNamer<> for this template class: " + plain);
  }
  assert(plain.find_first_of('>') == std::string::npos);

  // Remove any namespaces
  auto index = plain.find_last_of(':');
  if (index != std::string::npos) {
    return plain.substr(index + 1);
  }

  return plain;
}

}
