#include <pep/database/Storage.hpp>
#include <pep/storagefacility/Constants.hpp>
#include <pep/storagefacility/SqliteStorage.hpp>
#include <gtest/gtest.h>

namespace {

pep::PurgeCycle MakePurgeCycle(pep::Timestamp announced) {
  return pep::PurgeCycle{
    .id{},
    .definition = pep::PurgeCycle::Definition(pep::BackCatalogPurgeFilter),
    .announced = announced,
    .applied{},
    .completed{},
  };
}

}

TEST(SqliteStorage, Basics) {
  using pep::storagefacility::SqliteStorage;
  SqliteStorage storage(pep::database::BasicStorage::StoreInMemory);
  EXPECT_TRUE(storage.getPurgeCycles().empty());

  const auto announcement = pep::TimeNow();
  const auto cycle1id = storage.createPurgeCycle(MakePurgeCycle(announcement));
  EXPECT_EQ(storage.getPurgeCycles().size(), 1);

  auto cycle1 = storage.getPurgeCycle(cycle1id);
  EXPECT_EQ(cycle1.id, cycle1id);
  EXPECT_ANY_THROW(storage.getPurgeCycle(cycle1id + 1)) << "ID shouldn't exist since we only have a single cycle";

  const auto entryName = pep::EntryName(pep::LocalPseudonym::Random(), "c");
  const auto cellVersion0 = SqliteStorage::CellVersionSpec{
      .cell = entryName,
      .timestamp = pep::TimeNow()
  };
  EXPECT_ANY_THROW(storage.addToPurgeCycle(cycle1id + 1, cellVersion0)) << "Should refuse to add to nonexistent cycle";

  const auto cycle2id = storage.createPurgeCycle(MakePurgeCycle(announcement));
  EXPECT_EQ(storage.getPurgeCycles().size(), 2);

  storage.addToPurgeCycle(cycle1id, cellVersion0);
  auto versions = storage.getIncludedCellVersions(cycle1id);
  EXPECT_EQ(versions.size(), 1);
  EXPECT_EQ(*versions.begin(), cellVersion0);

  EXPECT_ANY_THROW(storage.addToPurgeCycle(cycle1id, cellVersion0));
  EXPECT_ANY_THROW(storage.addToPurgeCycle(cycle2id, cellVersion0));

  storage.addToPurgeCycle(cycle1id, SqliteStorage::CellVersionSpec{
      .cell = entryName,
      .timestamp = cellVersion0.timestamp + std::chrono::milliseconds(1)
    });
  storage.addToPurgeCycle(cycle1id, SqliteStorage::CellVersionSpec{
      .cell = entryName,
      .timestamp = cellVersion0.timestamp + std::chrono::milliseconds(2)
    });
  EXPECT_EQ(storage.getIncludedCellVersions(cycle1id).size(), 3);

  storage.addToPurgeCycle(cycle2id, SqliteStorage::CellVersionSpec{
      .cell = entryName,
      .timestamp = cellVersion0.timestamp + std::chrono::milliseconds(3)
    });
  EXPECT_EQ(storage.getIncludedCellVersions(cycle1id).size(), 3);
  EXPECT_EQ(storage.getIncludedCellVersions(cycle2id).size(), 1);

  storage.deletePurgeCycle(cycle2id);
  EXPECT_EQ(storage.getIncludedCellVersions(cycle1id).size(), 3);
  EXPECT_EQ(storage.getIncludedCellVersions(cycle2id).size(), 0);

  EXPECT_FALSE(storage.getPurgeCycle(cycle1id).applied.has_value());
  cycle1.applied = pep::TimeNow();
  storage.updatePurgeCycle(cycle1);
  EXPECT_TRUE(storage.getPurgeCycle(cycle1id).applied.has_value());
  EXPECT_EQ(storage.getIncludedCellVersions(cycle1id).size(), 3);

  EXPECT_FALSE(storage.getPurgeCycle(cycle1id).completed.has_value());
  cycle1.completed = pep::TimeNow();
  storage.updatePurgeCycle(cycle1);
  EXPECT_TRUE(storage.getPurgeCycle(cycle1id).completed.has_value());
  EXPECT_EQ(storage.getIncludedCellVersions(cycle1id).size(), 3);
}

TEST(SqliteStorage, Atomicity) {
  using pep::storagefacility::SqliteStorage;
  SqliteStorage storage(pep::database::BasicStorage::StoreInMemory);

  pep::PurgeCycle::Id cycle{};
  const auto cellVersion0 = SqliteStorage::CellVersionSpec{
      .cell = pep::EntryName(pep::LocalPseudonym::Random(), "c"),
      .timestamp = pep::TimeNow()
  };
  storage.atomically([&]() {
    cycle = storage.createPurgeCycle(MakePurgeCycle(pep::TimeNow()));
    storage.addToPurgeCycle(cycle, cellVersion0);
    });
  EXPECT_NE(cycle, pep::PurgeCycle::Id{});
  EXPECT_EQ(storage.getIncludedCellVersions(cycle).size(), 1);

  auto inserted = false;
  EXPECT_THROW(storage.atomically([&]() {
    storage.addToPurgeCycle(cycle, SqliteStorage::CellVersionSpec{
      .cell = cellVersion0.cell,
      .timestamp = cellVersion0.timestamp + std::chrono::milliseconds(1)
      });
    inserted = true;
    storage.addToPurgeCycle(cycle, cellVersion0); // Duplicate
    }), pep::storagefacility::SqliteStorage::ConstraintViolation);

  EXPECT_TRUE(inserted) << "Atomic operation failed the insertion that was supposed to succeed";
  EXPECT_EQ(storage.getIncludedCellVersions(cycle).size(), 1) << "Atomic operation should have rolled back successful insertion";
}
