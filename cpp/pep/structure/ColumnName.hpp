#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace pep {

class ColumnNameSection {
private:
  std::string mValue;

public:
  explicit ColumnNameSection(std::string value);

  inline const std::string& getValue() const noexcept { return mValue; }

  static ColumnNameSection FromRawString(const std::string& raw);
};

struct ColumnNameMapping {
  ColumnNameSection original;
  ColumnNameSection mapped;
};

class ColumnNameMappings {
private:
  std::unordered_map<std::string, ColumnNameMapping> mEntries;

public:
  explicit ColumnNameMappings(const std::vector<ColumnNameMapping>& entries);

  std::vector<ColumnNameMapping> getEntries() const;
  std::string getColumnNameSectionFor(const std::string& rawOriginal) const;
};

}
