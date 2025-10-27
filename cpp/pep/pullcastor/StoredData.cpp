#include <pep/async/RxDistinct.hpp>
#include <pep/async/RxRequireCount.hpp>
#include <pep/async/RxInstead.hpp>
#include <pep/async/RxSharedPtrCast.hpp>
#include <pep/core-client/CoreClient.hpp>
#include <pep/pullcastor/StoredData.hpp>

#include <rxcpp/operators/rx-filter.hpp>

namespace pep {
namespace castor {

StoredData::StoredData(std::shared_ptr<ParticipantsByColumnBoundParticipantId> participants)
  : mParticipants(participants) {
}

rxcpp::observable<std::shared_ptr<StoredData>> StoredData::Load(std::shared_ptr<CoreClient> client,
  std::shared_ptr<std::vector<PolymorphicPseudonym>> participants,
  std::shared_ptr<std::vector<std::string>> spColumns, std::shared_ptr<std::vector<std::string>> otherColumns) {

  auto mapped = std::make_shared<ParticipantsByColumnBoundParticipantId>();

  std::vector<std::string> participantGroups;
  if (participants->empty()) {
    participantGroups.emplace_back("*");
  }
  std::vector<std::string> columns;
  columns.reserve(spColumns->size() + otherColumns->size());
  columns.insert(columns.end(), spColumns->cbegin(), spColumns->cend());
  columns.insert(columns.end(), otherColumns->cbegin(), otherColumns->cend());

  return PepParticipant::LoadAll(client, *participants, participantGroups, columns, { })
    .flat_map([spColumns, mapped](std::shared_ptr<PepParticipant> participant) {
    return rxcpp::observable<>::iterate(*spColumns)
      .flat_map([mapped, participant](const std::string& column) -> rxcpp::observable<FakeVoid> {
      auto content = participant->tryGetCellContent(column);
      if (content == nullptr) {
        return rxcpp::observable<>::just(FakeVoid());
      }
      return content->getData()
        .op(RxGetOne("short pseudonym cell data"))
        .map([mapped, participant, column](const std::string& sp) {
        ColumnBoundParticipantId cbrId(column, sp);
        if (!mapped->emplace(std::make_pair(cbrId, participant)).second) {
          throw std::runtime_error("Duplicate Castor SP found: " + sp);
        }
        return FakeVoid();
        });
      });
    })
    .op(RxInstead(mapped))
    .map([](std::shared_ptr<ParticipantsByColumnBoundParticipantId> participants) {return StoredData::Create(participants); });
}

std::shared_ptr<PepParticipant> StoredData::tryGetParticipant(const ColumnBoundParticipantId& cbrId) const noexcept {
  auto found = mParticipants->find(cbrId);
  if (found == mParticipants->cend()) {
    return nullptr;
  }
  return found->second;
}

bool StoredData::hasCastorParticipantId(const ColumnBoundParticipantId& cbpId) const noexcept {
  return tryGetParticipant(cbpId) != nullptr;
}

rxcpp::observable<StoreData2Entry> StoredData::getUpdateEntry(std::shared_ptr<StorableCellContent> storable) {
  auto cbpId = storable->getColumnBoundParticipantId();
  auto participant = tryGetParticipant(cbpId);
  if (participant == nullptr) {
    throw std::runtime_error("No PEP participant record found for Castor participant ID " + cbpId.getParticipantId() + " in column " + cbpId.getColumnName());
  }

  auto column = storable->getColumn();
  auto existing = participant->tryGetCellContent(column);
  if (existing == nullptr) {
    PULLCASTOR_LOG(debug) << "Adding new cell to PEP.";
    return storable->getContent()->getData()
      .map([column, pp = std::make_shared<PolymorphicPseudonym>(participant->getPp()), extension = storable->getFileExtension()](const std::string& data) {
      return StoreData2Entry(pp, column, std::make_shared<std::string>(data), { MetadataXEntry::MakeFileExtension(extension) });
      });
  }

  auto updating = std::make_shared<bool>(false);
  return existing->getData()
    .flat_map([participant, storable](const std::string& data) -> rxcpp::observable<StoreData2Entry> {
    return storable->getContent()->getDataToStore(data)
      .map([participant, column = storable->getColumn(), extension = storable->getFileExtension()](const std::string& store) {
      return StoreData2Entry(std::make_shared<PolymorphicPseudonym>(participant->getPp()), column, std::make_shared<std::string>(store), {MetadataXEntry::MakeFileExtension(extension)});
      });
    })
    .tap(
      [updating](const StoreData2Entry&) { *updating = true; },
      [](std::exception_ptr) {},
      [updating]() {
        if (*updating) {
          PULLCASTOR_LOG(debug) << "Updating PEP cell with new content.";
        }
        else {
          PULLCASTOR_LOG(debug) << "Skipping cell that was already stored in PEP.";
        }
      }
    );
}

rxcpp::observable<std::shared_ptr<const PepParticipant>> StoredData::getParticipants() const {
  return rxcpp::observable<>::iterate(*mParticipants)
    .map([](const auto& pair) {return pair.second; })
    .op(RxDistinct())
    .op(RxSharedPtrCast<const PepParticipant>());
}

rxcpp::observable<std::string> StoredData::getCastorSps(std::shared_ptr<const PepParticipant> participant, const std::string& spColumnName) const {
  return rxcpp::observable<>::iterate(*mParticipants)
    .filter([participant, spColumnName](const auto& pair) {return pair.first.getColumnName() == spColumnName && pair.second == participant; })
    .map([](const auto& pair) {return pair.first.getParticipantId(); });
}

}
}
