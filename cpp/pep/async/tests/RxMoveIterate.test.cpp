#include <gtest/gtest.h>
#include <pep/async/RxIterate.hpp>

#include <rxcpp/operators/rx-map.hpp>

namespace {

struct NoCopy {
  NoCopy() = default;
  NoCopy(NoCopy&&) = default;
  NoCopy& operator=(NoCopy&&) = default;
  NoCopy(const NoCopy&) { throw std::runtime_error("Do not copy me"); }
};

TEST(RxIterate, NoCopy) {
  EXPECT_NO_THROW(pep::RxIterate(std::vector<NoCopy>(1))
    .map([](NoCopy v) { return v; })
    .subscribe([&](const NoCopy&) {}));
}

}
