#include <pep/structure/ColumnName.hpp>

#include <cassert>
#include <algorithm>
#include <iterator>
#include <regex>
#include <stdexcept>
#include <utility>

namespace {

std::string Mangle(const std::string& columnNameSection) {
  const std::string tmp = std::regex_replace(columnNameSection, std::regex("\\s"), "_");
  return std::regex_replace(tmp, std::regex("[^a-zA-Z0-9_]+"), "");
}

}

namespace pep {

ColumnNameSection::ColumnNameSection(std::string value)
  : mValue(std::move(value)) {
}

ColumnNameSection ColumnNameSection::FromRawString(const std::string& raw) {
  return ColumnNameSection(Mangle(raw));
}

ColumnNameMappings::ColumnNameMappings(const std::vector<ColumnNameMapping>& entries) {
  mEntries.reserve(entries.size());
  for (const auto& mapping : entries) {
    const auto& key = mapping.original.getValue();
    if (!mEntries.emplace(key, mapping).second) {
      throw std::runtime_error("Column name mapping could not be stored for \"" + key + "\". Were duplicate names provided?");
    }
  }
}

std::string ColumnNameMappings::getColumnNameSectionFor(const std::string& rawOriginal) const {
  auto original = ColumnNameSection::FromRawString(rawOriginal).getValue();
  auto position = mEntries.find(original);
  if (position != mEntries.cend()) {
    return position->second.mapped.getValue();
  }
  return original;
}

std::vector<ColumnNameMapping> ColumnNameMappings::getEntries() const {
  std::vector<ColumnNameMapping> result;
  result.reserve(mEntries.size());
  std::transform(mEntries.cbegin(), mEntries.cend(), std::back_inserter(result), [](const std::pair<const std::string, ColumnNameMapping>& entry) {return entry.second; });
  return result;
}

}
