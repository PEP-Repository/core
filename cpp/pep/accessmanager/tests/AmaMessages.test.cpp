#include <gtest/gtest.h>

#include <pep/accessmanager/AmaMessages.hpp>

namespace {

TEST(AmaQRColumnGroupFillColumnGroupToCapacity, simple) {
  // Arrange 
  pep::AmaQRColumnGroup source{"cgName", std::vector<std::string>{"col1", "col2", "col3"}};
  pep::AmaQRColumnGroup dest{};
  size_t capactity{1024};
  size_t offset{0};

  //Act
  size_t actualLength = pep::AmaQRColumnGroup::FillToProtobufSerializationCapacity(dest, source, capactity, offset);

  //Assert
  pep::AmaQRColumnGroup expectedCG{"cgName", std::vector<std::string>{"col1", "col2", "col3"}};
  ASSERT_EQ(dest.mName, expectedCG.mName);
  ASSERT_EQ(dest.mColumns, expectedCG.mColumns);
  ASSERT_EQ(actualLength, 26U);
}

TEST(AmaQRColumnGroupFillColumnGroupToCapacity, capacityZero) {
  // Arrange 
  pep::AmaQRColumnGroup source{"cgName", std::vector<std::string>{"col1", "col2", "col3"}};
  pep::AmaQRColumnGroup dest{};
  size_t capactity{0};
  size_t offset{0};

  //Act
  size_t actualLength = pep::AmaQRColumnGroup::FillToProtobufSerializationCapacity(dest, source, capactity, offset);

  //Assert
  ASSERT_EQ(dest.mName, "");
  ASSERT_EQ(dest.mColumns, std::vector<std::string>{});
  ASSERT_EQ(actualLength, 0U);
}


TEST(AmaQRColumnGroupFillColumnGroupToCapacity, CapacityLimited) {
  // Arrange 
  pep::AmaQRColumnGroup source{"cgName", std::vector<std::string>{"col1", "col2", "col3"}};
  pep::AmaQRColumnGroup dest{};
  size_t capactity{16};
  size_t offset{0};

  //Act
  size_t actualLength = pep::AmaQRColumnGroup::FillToProtobufSerializationCapacity(dest, source, capactity, offset);

  //Assert
  pep::AmaQRColumnGroup expectedCG{"cgName", std::vector<std::string>{"col1"}};
  ASSERT_EQ(expectedCG.mName, dest.mName);
  ASSERT_EQ(expectedCG.mColumns, dest.mColumns);
  ASSERT_EQ(actualLength, 14U);
}

TEST(AmaQRColumnGroupFillColumnGroupToCapacity, OffsetLimited) {
  // Arrange 
  pep::AmaQRColumnGroup source{"cgName", std::vector<std::string>{"col1", "col2", "col3"}};
  pep::AmaQRColumnGroup dest{};
  size_t capactity{1024};
  size_t offset{2};

  //Act
  size_t actualLength = pep::AmaQRColumnGroup::FillToProtobufSerializationCapacity(dest, source, capactity, offset);

  //Assert
  pep::AmaQRColumnGroup expectedCG{"cgName", std::vector<std::string>{"col3"}};
  ASSERT_EQ(expectedCG.mName, dest.mName);
  ASSERT_EQ(expectedCG.mColumns, dest.mColumns);
  ASSERT_EQ(actualLength, 14U);
}

TEST(AmaQRColumnGroupFillColumnGroupToCapacity, OffsetAndCapacity) {
  // Arrange 
  pep::AmaQRColumnGroup source{"cgName", std::vector<std::string>{"col1", "col2", "col3", "col4"}};
  pep::AmaQRColumnGroup dest{};
  size_t capactity{16};
  size_t offset{2};

  //Act
  size_t actualLength = pep::AmaQRColumnGroup::FillToProtobufSerializationCapacity(dest, source, capactity, offset);

  //Assert
  pep::AmaQRColumnGroup expectedCG{"cgName", std::vector<std::string>{"col3"}};
  ASSERT_EQ(expectedCG.mName, dest.mName);
  ASSERT_EQ(expectedCG.mColumns, dest.mColumns);
  ASSERT_EQ(actualLength, 14U);
}


TEST(AmaQRColumnGroupFillColumnGroupToCapacity, NoPadding) {
  // Arrange 
  pep::AmaQRColumnGroup source{"cgName", std::vector<std::string>{"col1", "col2", "col3"}};
  pep::AmaQRColumnGroup dest{};
  size_t capactity{1024};
  size_t offset{0};
  size_t padding{0};

  //Act
  size_t actualLength = pep::AmaQRColumnGroup::FillToProtobufSerializationCapacity(dest, source, capactity, offset, padding);

  //Assert
  pep::AmaQRColumnGroup expectedCG{"cgName", std::vector<std::string>{"col1", "col2", "col3"}};
  ASSERT_EQ(dest.mName, expectedCG.mName);
  ASSERT_EQ(dest.mColumns, expectedCG.mColumns);
  ASSERT_EQ(actualLength, 18U);
}

}
