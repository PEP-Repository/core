#include <pep/storagefacility/Constants.hpp>
#include <pep/storagefacility/PurgeTypes.hpp>
#include <stdexcept>

namespace pep {

PurgeCycleDefinition::PurgeCycleDefinition(std::string name)
  : name_(std::move(name)) {
  if (!PredefinedPurgeFilters.contains(name_)) {
    throw std::runtime_error("\"" + name_ + "\" is not a predefined purge filter");
  }
}

}
