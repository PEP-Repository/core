#include <pep/storagefacility/PagePathResponseChunker.hpp>

#include <span>
#include <stdexcept>
#include <utility>

namespace pep {

namespace {

// Upper bound for the protobuf encoding overhead of a length-delimited field (i.e. a string or submessage): a single-byte
// tag (since the relevant field numbers are < 16) plus a length varint of at most 5 bytes (since lengths are < 2^32).
constexpr size_t MaxFieldOverhead = 1 + 5;
// A PrefixedStringsEntry is stored as a (length-delimited) "PagePathResponse.paths" element, containing a (length-delimited) "oneof" field.
constexpr size_t EntryOverhead = 2 * MaxFieldOverhead;
// A PopPrefix is an entry without content.
constexpr size_t PopPrefixSize = EntryOverhead;

[[nodiscard]] size_t StringSize(const std::string& str) {
  return MaxFieldOverhead + str.size();
}

[[nodiscard]] size_t PushPrefixSize(const std::string& prefix) {
  return EntryOverhead + StringSize(prefix);
}

[[nodiscard]] bool EndsWithPrefixedStrings(const PagePathResponse& chunk) {
  return !chunk.paths.empty() && std::holds_alternative<PrefixedStrings>(chunk.paths.back());
}

/// \brief Assembles a chunk whose serialized size doesn't exceed a specified capacity.
/// \details Pushes prefixes onto the chunk only when a string is added under them. Reserves space to pop all pushed
///          prefixes, so that the chunk can always be closed.
class ChunkBuilder {
  size_t capacity_;
  std::vector<std::string> prefixes_; // All prefixes that are currently active
  size_t pushedPrefixCount_{0}; // The number of (leading) prefixes_ that have been pushed onto chunk_
  PagePathResponse chunk_;
  size_t chunkSize_{0}; // Upper bound for the serialized size of chunk_

  [[nodiscard]] size_t requiredSizeToAdd(const std::string& str) const {
    auto result = StringSize(str);
    if (pushedPrefixCount_ < prefixes_.size() || !EndsWithPrefixedStrings(chunk_)) {
      // We'll need to push prefixes and/or start a new PrefixedStrings entry
      for (auto i = pushedPrefixCount_; i < prefixes_.size(); ++i) {
        result += PushPrefixSize(prefixes_[i]);
      }
      result += EntryOverhead;
    }
    return result;
  }

public:
  explicit ChunkBuilder(size_t capacity) : capacity_(capacity) {}

  [[nodiscard]] bool empty() const noexcept { return chunk_.paths.empty(); }

  void pushPrefix(std::string prefix) {
    prefixes_.push_back(std::move(prefix)); // Pushed onto the chunk when (if) a string is added under it
  }

  void popPrefix() {
    if (prefixes_.empty()) {
      throw std::logic_error("Cannot pop prefix: no prefix has been pushed");
    }
    if (pushedPrefixCount_ == prefixes_.size()) {
      chunk_.paths.emplace_back(PopPrefix{}); // Fits because tryAdd reserved space to pop every pushed prefix
      chunkSize_ += PopPrefixSize;
      --pushedPrefixCount_;
    }
    prefixes_.pop_back();
  }

  /// \brief Adds a string to the chunk if it fits.
  /// \return Whether the string was added (and moved from).
  [[nodiscard]] bool tryAdd(std::string& str) {
    // Ensure that we can (later) pop all prefixes that will have been pushed after adding this string
    auto popReservation = prefixes_.size() * PopPrefixSize;
    auto required = this->requiredSizeToAdd(str);
    if (chunkSize_ + required + popReservation > capacity_) {
      return false;
    }

    for (; pushedPrefixCount_ < prefixes_.size(); ++pushedPrefixCount_) {
      chunk_.paths.emplace_back(PushPrefix{.prefix = prefixes_[pushedPrefixCount_]});
    }
    if (!EndsWithPrefixedStrings(chunk_)) {
      chunk_.paths.emplace_back(PrefixedStrings{});
    }
    std::get<PrefixedStrings>(chunk_.paths.back()).strings.push_back(std::move(str));
    chunkSize_ += required;
    return true;
  }

  /// \brief Closes the chunk (popping the prefixes it pushed) and returns it, starting a new one.
  PagePathResponse take() {
    for (; pushedPrefixCount_ > 0U; --pushedPrefixCount_) {
      chunk_.paths.emplace_back(PopPrefix{});
    }
    chunkSize_ = 0U;
    return std::exchange(chunk_, PagePathResponse{});
  }

  void ensureAllPrefixesPopped() const {
    if (!prefixes_.empty()) {
      throw std::runtime_error("Not all prefixes have been popped");
    }
  }
};

}

// We take `entries` as Generator so that we can implement this in a cpp file.
Generator<PagePathResponse> ChunkPagePaths(Generator<PrefixedStringsEntry> entries, size_t capacity) {
  ChunkBuilder builder(capacity);

  for (auto&& entry : entries) {
    if (auto* push = std::get_if<PushPrefix>(&entry)) {
      builder.pushPrefix(std::move(push->prefix));
    }
    else if (std::holds_alternative<PopPrefix>(entry)) {
      builder.popPrefix();
    }
    else {
      auto* single = std::get_if<PrefixedString>(&entry);
      auto strings = single != nullptr ? std::span(&single->string, 1U) : std::span(std::get<PrefixedStrings>(entry).strings);
      for (auto& str : strings) {
        if (!builder.tryAdd(str)) {
          if (!builder.empty()) {
            co_yield builder.take();
          }
          if (!builder.tryAdd(str)) {
            throw std::runtime_error("String (with its prefixes) is too large to fit into a single chunk");
          }
        }
      }
    }
  }

  builder.ensureAllPrefixesPopped();
  if (!builder.empty()) {
    co_yield builder.take();
  }
}

}
