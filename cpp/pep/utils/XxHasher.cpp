#include <pep/utils/XxHasher.hpp>

#include <cassert>

namespace pep {

void XxHasher::verifyActive() const {
  if (state_ == nullptr) {
    throw std::runtime_error("Hashing has already been completed");
  }
}

void XxHasher::discardState() noexcept {
  if (state_ != nullptr) {
    [[maybe_unused]] auto error = XXH64_freeState(state_);
    assert(error == XXH_OK); // Documented as such

    state_ = nullptr;
  }
}

XxHasher::XxHasher(XXH64_hash_t seed) : state_(XXH64_createState()) {
  if (state_ == nullptr) {
    throw std::runtime_error("Could not create XXH64 state");
  }
  if (XXH64_reset(state_, seed) != XXH_OK) {
    discardState();
    throw std::runtime_error("Error resetting XXH64 state");
  }
}

void XxHasher::process(const void* block, size_t size) {
  verifyActive();
  if (XXH64_update(state_, block, size) != XXH_OK) {
    discardState();
    throw std::runtime_error("Error processing XXH64 block");
  }
}

XxHasher::Hash XxHasher::finish() {
  verifyActive();
  auto result = XXH64_digest(state_);
  discardState();
  return result;
}

XxHasher::~XxHasher() noexcept {
  discardState();
}

}
