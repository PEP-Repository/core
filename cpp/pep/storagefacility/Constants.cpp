#include <pep/storagefacility/Constants.hpp>
#include <set>
#include <string>

namespace pep {

extern const std::string BackCatalogRedactionFilter = "Back Catalog";

extern const std::set<std::string> PredefinedRedactionFilters{
  BackCatalogRedactionFilter
};

}
