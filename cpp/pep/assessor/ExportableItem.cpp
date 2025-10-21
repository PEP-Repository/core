#include <pep/assessor/ExportableItem.hpp>
#include <pep/content/ParticipantDeviceHistory.hpp>
#include <pep/crypto/Timestamp.hpp>

#include <cassert>

ExportableShortPseudonymItem::ExportableShortPseudonymItem(const pep::ShortPseudonymDefinition& definition)
  : mColumnName(definition.getColumn().getFullName()), mDescription(definition.getDescription()), mVisitNumber(definition.getColumn().getVisitNumber()) {
}

ExportableDeviceHistoryItem::ExportableDeviceHistoryItem(const std::string& columnName, const std::optional<std::string>& description)
  : mColumnName(columnName), mDescription(description) {
}
ExportableVisitAssessorItem::ExportableVisitAssessorItem(const std::string& columnName, const unsigned int visitNumber) :
  mColumnName(columnName), mVisitNumber(visitNumber) {
}

std::optional<std::function<void(ExportDataTable&, const std::optional<std::string>& value)>> ExportableDeviceHistoryItem::getDetailExpander() const {
  return [](ExportDataTable& destination, const std::optional<std::string>& value) {
    const auto CELLS = 3U;

    if (value) {
      auto history = pep::ParticipantDeviceHistory::Parse(*value, false);
      for (const auto& entry : history) {
        auto& row = destination.emplace_back();

        row.emplace_back(entry.type);
        row.emplace_back(entry.serial);
        row.emplace_back(pep::TimestampToXmlDateTime(entry.time));

        assert(row.size() == CELLS);
      }
    }
    else {
      auto& row = destination.emplace_back();
      for (auto i = 0U; i < CELLS; ++i) {
        row.emplace_back(std::nullopt);
      }
    }
  };
}
