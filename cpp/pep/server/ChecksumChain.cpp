#include <pep/serialization/Error.hpp>
#include <pep/server/ChecksumChain.hpp>
#include <ranges>
#include <stdexcept>

namespace pep {

void ChecksumChains::insert(std::unique_ptr<ChecksumChain> chain) {
  auto result = items_.emplace(std::move(chain));
  if (!result.second) { // Not inserted
    throw std::runtime_error("Duplicate checksum chain name: " + (*result.first)->name());
  }
}

std::set<std::string> ChecksumChains::names() const {
  return items_
    | std::views::transform([](const auto& chain) { return chain->name(); })
    | std::ranges::to<std::set>();
}

const ChecksumChain& ChecksumChains::chain(const std::string& name) const {
  auto position = items_.find(name);
  if (position == items_.end()) {
    throw Error("No checksum chain available with name " + name);
  }
  return **position;
}

}
