#pragma once

#include <pep/storagefacility/StorageFacilityMessages.hpp>

#include <pep/utils/Generator.hpp>

namespace pep {

/// \brief Combines a sequence of prefixed strings (such as page paths) into PagePathResponse chunks, each of which
///        serializes to (at most) a specified number of bytes.
/// \param entries The prefixed strings to combine. Every PushPrefix must be matched by a (later) PopPrefix.
/// \param capacity The maximum serialized size of a chunk.
/// \return The chunks. (Consecutive) strings are combined into PrefixedStrings entries. Every chunk is self-contained:
///         it pushes the prefixes that apply to its strings, and pops them again at its end. Prefixes are only included
///         in a chunk if strings are added under them, so (parts of) the input without strings don't take up space.
/// \remark Throws (when iterating) std::runtime_error if a string (with its prefixes) doesn't fit into a single chunk,
///         or std::logic_error if the input's PushPrefix and PopPrefix entries don't match up.
[[nodiscard]] Generator<PagePathResponse> ChunkPagePaths(Generator<PrefixedStringsEntry> entries, size_t capacity);

}
