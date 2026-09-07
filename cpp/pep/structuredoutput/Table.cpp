#include <cassert>
#include <iterator>
#include <pep/structuredoutput/Table.hpp>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <vector>

namespace pep::structuredOutput {

Table::RecordRef Table::emplace_back(std::vector<std::string> record) {
  if (record.size() != recordSize_) {
    throw std::runtime_error{"Error extending Table: size does not match the record size."};
  }

  data_.insert(data_.end(), std::make_move_iterator(record.begin()), std::make_move_iterator(record.end()));

  const auto lastRecordBegin = data_.end() - static_cast<std::ptrdiff_t>(recordSize_);
  //NOLINTNEXTLINE(bugprone-dangling-handle) Caller should be careful to not access after Table destruction or modification
  return {lastRecordBegin, data_.end()};
}

std::vector<Table::ConstRecordRef> Table::records() const noexcept {
  assert(data_.size() % recordSize_ == 0); // implementation relies on this class invariant

  std::vector<ConstRecordRef> spans;
  spans.reserve(data_.size() / recordSize_);
  const auto stepSize = static_cast<std::ptrdiff_t>(recordSize_);
  for (auto i = data_.cbegin(); i != data_.cend(); i += stepSize) { spans.emplace_back(i, i + stepSize); }
  return spans;
}

Table::Table(std::vector<std::string> header, std::vector<std::string> data, std::size_t recordSize)
  : header_{std::move(header)}, data_{std::move(data)}, recordSize_{recordSize} {
  assert(header_.size() == recordSize); // Public API should not allow this to fail

  if (recordSize_ == 0) { throw std::runtime_error{"Error creating Table: record size cannot be 0."}; }

  if (data_.size() % recordSize_ != 0) {
    throw std::runtime_error{"Error creating Table: number of fields is not a multiple of the record size."};
  }
}

Table::RecordRef Table::asMutable(ConstRecordRef ref) {
  assert(!ref.empty());
  assert([&] { // require that `this` owns the refered data
    const auto contains = [](auto& large, auto& small) {
      if (large.empty()) {
        return false;
      }
      auto lastsmall = small.end() - 1;
      auto lastlarge = large.end() - 1;
      // TODO: less-than comparison of unrelated pointers is UB. See https://stackoverflow.com/a/4909929
      return &*large.begin() <= &*small.begin() && &*lastsmall <= &*lastlarge; // &* converts iterators to the same ptr type
    };
    return contains(data_, ref) || contains(header_, ref);
  }());
  //NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast) Safe: this has access to non-const data_
  return {const_cast<decltype(data_.data())>(ref.data()), ref.size()};
}

std::vector<Table::RecordRef> Table::asMutable(const std::vector<ConstRecordRef>& cRefs) {
  const auto toMutable = [this](const ConstRecordRef& r) { return this->asMutable(r); };
  const auto refs_ = std::views::transform(cRefs, toMutable);
  return {refs_.begin(), refs_.end()};
}

} // namespace pep::structuredOutput
