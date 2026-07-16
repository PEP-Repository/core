#include <pep/storagefacility/Constants.hpp>
#include <pep/storagefacility/PurgeMessages.hpp>

namespace pep {

PurgeCycleRequest::PurgeCycleRequest(std::string name, bool predefined)
  : name_(std::move(name)), predefined_(predefined) {
  if (predefined_ != PredefinedPurgeFilters.contains(name_)) {
    throw std::runtime_error("\"" + name_ + "\" is " + (predefined_ ? "not " : "") + " a predefined purge filter");
  }
}

}
