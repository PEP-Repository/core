#pragma once

#include <pep/utils/Timestamp.hpp>
#include <cstdint>
#include <string>

namespace pep {

struct PurgeCycle {
  using Id = uint32_t; // An unsigned integer that (fits Sqlite's int64_t integer API and) uniquely refers to a purge cycle

  class Definition {
  private:
    std::string name_;

  public:
    explicit Definition(std::string name);

    const std::string& name() const& noexcept { return name_; }
    std::string&& name() && noexcept { return std::move(name_); }
  };

  Id id;
  Definition definition;
  Timestamp announced;
};

}
