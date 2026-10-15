#include <pep/serialization/Error.hpp>
#include <pep/server/ChecksumChain.hpp>
#include <pep/utils/Bitpacking.hpp>
#include <ranges>
#include <stdexcept>

namespace pep {

namespace {

constexpr size_t FormattedChecksumChainValueLength = 8U;

std::string ChecksumChainValueToString(uint64_t value) {
  auto result = PackUint64BE(value);
  assert(result.size() == FormattedChecksumChainValueLength);
  return result;
}

std::optional<uint64_t> ChecksumChainValueFromString(const std::string& str) {
  std::optional<uint64_t> result;
  if (!str.empty()) {
    if (str.size() != FormattedChecksumChainValueLength) {
      throw Error("checkpoint field should either be " + std::to_string(FormattedChecksumChainValueLength) + " bytes or 0");
    }
    result = UnpackUint64BE(str);
  }
  return result;
}

}

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

[[noreturn]] void ChecksumChains::ThrowUnknownChainError(const std::string& name) {
  throw Error("No checksum chain available with name " + name);
}

const ChecksumChain& ChecksumChains::chain(const std::string& name) const {
  auto position = items_.find(name);
  if (position == items_.end()) {
    ThrowUnknownChainError(name);
  }
  return **position;
}

}
