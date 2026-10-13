#pragma once

#include <cstdint>

namespace pep {

struct ChecksumChain {
  using Checksum = uint64_t;
  using Checkpoint = uint64_t;

  struct Result {
    Checksum checksum = 0;
    Checkpoint checkpoint = 0;
  };
};

}
