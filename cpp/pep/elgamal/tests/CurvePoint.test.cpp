#include <pep/elgamal/CurvePoint.hpp>
#include <pep/elgamal/CurveScalar.hpp>

#include <gtest/gtest.h>

namespace {

TEST(CurvePointTest, TestConstructor) {
  pep::CurvePoint point;
}

TEST(CurvePointTest, TestCompare) {
  pep::CurvePoint pointA;
  EXPECT_EQ(pointA, pointA) << "Point is not equal to itself";
  pep::CurvePoint pointB = pep::CurvePoint::Random();
  EXPECT_EQ(pointB, pointB) << "Point is not equal to itself";
  EXPECT_NE(pointA, pointB) << "Different points are equal";
}

TEST(CurvePointTest, TestRandom) {
  pep::CurvePoint pointA = pep::CurvePoint::Random();
  pep::CurvePoint pointB = pep::CurvePoint::Random();
  EXPECT_NE(pointA, pointB) << "Random points are equal";
}

TEST(CurvePointTest, TestAdd) {
  pep::CurvePoint pointA = pep::CurvePoint::Random();
  pep::CurvePoint pointB = pep::CurvePoint::Random();
  [[maybe_unused]] pep::CurvePoint pointC = pointA.add(pointB);
}

TEST(CurvePointTest, TestSub) {
  pep::CurvePoint pointA = pep::CurvePoint::Random();
  pep::CurvePoint pointB = pep::CurvePoint::Random();
  [[maybe_unused]] pep::CurvePoint pointC = pointA.sub(pointB);
}

TEST(CurvePointTest, TestFromText) {
  EXPECT_THROW(pep::CurvePoint::FromText(""), std::invalid_argument);
}

TEST(CurvePointTest, TestPrecomputedTable) {
  for (int i = 0; i < 100; i++) {
    auto pt = pep::CurvePoint::Random();
    auto s = pep::CurveScalar::Random();
    pep::CurvePoint::ScalarMultTable table(pt);
    EXPECT_EQ(pt.mult(s), table.mult(s));
  }
}

TEST(CurvePointTest, TestBaseMult) {
  const auto base = pep::CurvePoint::FromText("e2f2ae0a6abc4e71a884a961c500515f58e30b6aa582dd8db6a65945e08d2d76");
  for (int i = 0; i < 1000; i++) {
    pep::CurveScalar s = pep::CurveScalar::Random();
    EXPECT_EQ(pep::CurvePoint::BaseMult(s), base.mult(s));
  }
}

TEST(CurvePointTest, TestPublicBaseMult) {
  for (int i = 0; i < 1000; i++) {
    pep::CurveScalar s = pep::CurveScalar::Random();
    EXPECT_EQ(pep::CurvePoint::BaseMult(s), pep::CurvePoint::PublicBaseMult(s));
  }
}

TEST(CurvePointTest, TestPublicMult) {
  for (int i = 0; i < 1000; i++) {
    pep::CurvePoint b = pep::CurvePoint::Random();
    pep::CurveScalar s = pep::CurveScalar::Random();
    EXPECT_EQ(b.mult(s), b.publicMult(s));
  }
}

TEST(CurvePointTest, TestAddSub) {
  pep::CurvePoint pointA = pep::CurvePoint::Random();
  pep::CurvePoint pointB = pep::CurvePoint::Random();
  EXPECT_NE(pointA, pointB) << "Random points are equal";
  pep::CurvePoint pointC = pointA.add(pointB);
  pep::CurvePoint pointD = pointC.sub(pointB);
  EXPECT_EQ(pointD, pointA) << "Point A + B - B is not equal to A";
}

TEST(CurvePointTest, TestHash) {
  EXPECT_EQ(
    pep::CurvePoint::Hash("test"),
    pep::CurvePoint::FromText("b01d60504aa5f4c5bd9a7541c457661f9a789d18cb4e136e91d3c953488bd208"))
      << "CurvePoint::Hash(\"test\") is invalid";
  EXPECT_EQ(
    pep::CurvePoint::Hash("pep"),
    pep::CurvePoint::FromText("3286c8d171dec02e70549c280d62524430408a781efc07e4428d1735671d195b"))
      << "CurvePoint::Hash(\"pep\") is invalid";
  EXPECT_EQ(
    pep::CurvePoint::Hash("ristretto"),
    pep::CurvePoint::FromText("c2f6bb4c4dab8feab66eab09e77e79b36095c86b3cd1145b9a2703205858d712"))
      << "CurvePoint::Hash(\"ristretto\") is invalid";
  EXPECT_EQ(
    pep::CurvePoint::Hash("elligator"),
    pep::CurvePoint::FromText("784c727b1e8099eb94e5a8edbd260363567fdbd35106a7a29c8b809cd108b322"))
      << "CurvePoint::Hash(\"elligator\") is invalid";
}

}
