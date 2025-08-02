#include <gtest/gtest.h>

#include <pep/utils/Defer.hpp>

namespace {

TEST(Defer, Defer) {
  int did = 0;
  {
    PEP_DEFER(did++);

    ASSERT_EQ(did, 0);
  }
  ASSERT_EQ(did, 1);
}

TEST(Defer, DeferUnique) {
  int did = 0;
  {
    auto guard = defer_unique([&did]{ did++; });

    ASSERT_EQ(did, 0);
  }
  ASSERT_EQ(did, 1);

  {
    auto guard = defer_unique([&did]{ did++; });

    ASSERT_EQ(did, 1);
    guard.reset();
    ASSERT_EQ(did, 2);
  }

  {
    auto guard = defer_unique([&did]{ did++; });
    ASSERT_EQ(did, 2);
    auto guard2 = std::move(guard);
    ASSERT_EQ(did, 2);
  }
  ASSERT_EQ(did, 3);
}

TEST(Defer, DeferShared) {
  int did = 0;
  {
    auto guard = defer_shared([&did]{ did++; });

    ASSERT_EQ(did, 0);
  }
  ASSERT_EQ(did, 1);

  {
    auto guard = defer_shared([&did]{ did++; });

    ASSERT_EQ(did, 1);
    guard.reset();
    ASSERT_EQ(did, 2);
  }

  {
    auto guard = defer_shared([&did]{ did++; });
    ASSERT_EQ(did, 2);
    auto guard2 = guard;
    ASSERT_EQ(did, 2);
    guard.reset();
    ASSERT_EQ(did, 2);
  }
  ASSERT_EQ(did, 3);
}

}
