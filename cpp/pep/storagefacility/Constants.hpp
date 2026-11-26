#pragma once

#include <cstdint>
#include <set>
#include <string>

namespace pep {

constexpr uint64_t InlinePageThreshold = 4*1024;

extern const std::string BackCatalogRedactionFilter;
extern const std::set<std::string> PredefinedRedactionFilters;

}

