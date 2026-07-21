#include <pep/storagefacility/Constants.hpp>
#include <pep/storagefacility/PurgeCycle.hpp>
#include <stdexcept>

namespace pep {

PurgeCycle::Definition::Definition(std::string name)
  : name_(std::move(name)) {
  if (!PredefinedPurgeFilters.contains(name_)) {
    throw std::runtime_error("\"" + name_ + "\" is not a predefined purge filter");
  }
}

}
