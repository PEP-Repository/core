#include <pep/utils/XxHasher.hpp>

namespace pep {

void XxHasher::verifyActive() const {
  if (mState == nullptr) {
    throw std::runtime_error("Hashing has already been completed");
  }
}

void XxHasher::discardState() noexcept {
  if (mState != nullptr) {
    [[maybe_unused]] auto error = XXH64_freeState(mState);
    assert(error == XXH_OK); // Documented as such

    mState = nullptr;
  }
}

XxHasher::XxHasher(XXH64_hash_t seed) : mState(XXH64_createState()) {
  if (mState == nullptr) {
    throw std::runtime_error("Could not create XXH64 state");
  }
  if (XXH64_reset(mState, seed) != XXH_OK) {
    discardState();
    throw std::runtime_error("Error resetting XXH64 state");
  }
}

void XxHasher::process(const void* block, size_t size) {
  verifyActive();
  if (XXH64_update(mState, block, size) != XXH_OK) {
    discardState();
    throw std::runtime_error("Error processing XXH64 block");
  }
}

XxHasher::Hash XxHasher::finish() {
  verifyActive();
  auto result = XXH64_digest(mState);
  discardState();
  return result;
}

XxHasher::~XxHasher() noexcept {
  discardState();
}

}
