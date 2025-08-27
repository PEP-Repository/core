#include <cassert>
#include <iterator>
#include <pep/structuredoutput/Table.hpp>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <vector>

namespace pep::structuredOutput {

Table::RecordRef Table::emplace_back(std::vector<std::string> record) {
  if (record.size() != mRecordSize) {
    throw std::runtime_error{"Error extending Table: size does not match the record size."};
  }

  mData.insert(mData.end(), std::make_move_iterator(record.begin()), std::make_move_iterator(record.end()));

  const auto lastRecordBegin = mData.end() - static_cast<std::ptrdiff_t>(mRecordSize);
  return {lastRecordBegin, mData.end()};
}

std::vector<Table::ConstRecordRef> Table::records() const noexcept {
  assert(mData.size() % mRecordSize == 0); // implementation relies on this class invariant

  std::vector<ConstRecordRef> spans;
  spans.reserve(mData.size() / mRecordSize);
  const auto stepSize = static_cast<std::ptrdiff_t>(mRecordSize);
  for (auto i = mData.cbegin(); i != mData.cend(); i += stepSize) { spans.emplace_back(i, i + stepSize); }
  return spans;
}

Table::Table(std::vector<std::string> header, std::vector<std::string> data, std::size_t recordSize)
  : mHeader{std::move(header)}, mData{std::move(data)}, mRecordSize{recordSize} {
  assert(mHeader.size() == recordSize); // Public API should not allow this to fail

  if (mRecordSize == 0) { throw std::runtime_error{"Error creating Table: record size cannot be 0."}; }

  if (mData.size() % mRecordSize != 0) {
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
    return contains(mData, ref) || contains(mHeader, ref);
  }());
  //NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast) Safe: this has access to non-const mData
  return {const_cast<decltype(mData.data())>(ref.data()), ref.size()};
}

std::vector<Table::RecordRef> Table::asMutable(const std::vector<ConstRecordRef>& cRefs) {
  const auto toMutable = [this](const ConstRecordRef& r) { return this->asMutable(r); };
  const auto mRefs = std::views::transform(cRefs, toMutable);
  return {mRefs.begin(), mRefs.end()};
}

} // namespace pep::structuredOutput
