#include <pep/assessor/ExportableItem.hpp>
#include <pep/content/ParticipantDeviceHistory.hpp>
#include <pep/utils/Timestamp.hpp>

#include <cassert>

ExportableShortPseudonymItem::ExportableShortPseudonymItem(const pep::ShortPseudonymDefinition& definition)
  : columnName_(definition.getColumn().getFullName()), description_(definition.getDescription()), visitNumber_(definition.getColumn().getVisitNumber()) {
}

ExportableDeviceHistoryItem::ExportableDeviceHistoryItem(const std::string& columnName, const std::optional<std::string>& description)
  : columnName_(columnName), description_(description) {
}
ExportableVisitAssessorItem::ExportableVisitAssessorItem(const std::string& columnName, const unsigned int visitNumber) :
  columnName_(columnName), visitNumber_(visitNumber) {
}

std::optional<std::function<void(ExportDataTable&, const std::optional<std::string>& value)>> ExportableDeviceHistoryItem::getDetailExpander() const {
  return [](ExportDataTable& destination, const std::optional<std::string>& value) {
    const auto Cells = 3U;

    if (value) {
      auto history = pep::ParticipantDeviceHistory::Parse(*value, false);
      for (const auto& entry : history) {
        auto& row = destination.emplace_back();

        row.emplace_back(entry.type);
        row.emplace_back(entry.serial);
        row.emplace_back(pep::TimestampToXmlDateTime(entry.time));

        assert(row.size() == Cells);
      }
    }
    else {
      auto& row = destination.emplace_back();
      for (auto i = 0U; i < Cells; ++i) {
        row.emplace_back(std::nullopt);
      }
    }
  };
}
