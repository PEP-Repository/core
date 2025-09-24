#pragma once

#include <xxhash.h>

#include <pep/utils/Hasher.hpp>

namespace pep {

class XxHasher : public Hasher<XXH64_hash_t> {
public:
  using Hash = XXH64_hash_t;

private:
  XXH64_state_t* mState;

private:
  void verifyActive() const;
  void discardState() noexcept;

protected:
  void process (const void* block, size_t size) override;
  Hash finish() override;

public:
  XxHasher(XXH64_hash_t seed);
  ~XxHasher() noexcept override;
};

}
