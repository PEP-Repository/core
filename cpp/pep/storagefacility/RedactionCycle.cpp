#include <pep/storagefacility/Constants.hpp>
#include <pep/storagefacility/RedactionCycle.hpp>
#include <stdexcept>
#include <string>
#include <utility>

namespace pep {

RedactionCycle::Definition::Definition(std::string name)
  : name_(std::move(name)) {
  if (!PredefinedRedactionFilters.contains(name_)) {
    throw std::runtime_error("\"" + name_ + "\" is not a predefined redaction filter");
  }
}

}
