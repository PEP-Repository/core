#pragma once

#include <cstdint>
#include <string>

namespace pep {

using PurgeCycleId = uint32_t; // An unsigned integer that (fits Sqlite's int64_t integer API and) uniquely refers to a purge cycle (definition)

class PurgeCycleDefinition {
private:
  std::string name_;

public:
  explicit PurgeCycleDefinition(std::string name);

  const std::string& name() const& noexcept { return name_; }
  std::string&& name() && noexcept { return std::move(name_); }
};

}
