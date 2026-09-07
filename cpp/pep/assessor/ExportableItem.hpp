#pragma once
#include <pep/structure/GlobalConfiguration.hpp>
#include <pep/structure/ShortPseudonyms.hpp>

using ExportDataCell = std::optional<std::string>;
using ExportDataRow = std::vector<ExportDataCell>;
using ExportDataTable = std::vector<ExportDataRow>;
using ExportDataTableIterator = std::vector<ExportDataTable>::const_iterator;

class ExportableItem : public std::enable_shared_from_this<ExportableItem> {
public:
  virtual ~ExportableItem() = default;

  virtual std::string getSourceColumnName() const = 0;
  virtual std::string getCaptionPrefix() const = 0;
  virtual std::optional<std::string> getDescription() const { return std::nullopt; }
  virtual std::optional<unsigned int> getVisitNumber() const { return std::nullopt; }

  virtual std::optional<std::function<void(ExportDataTable&, const std::optional<std::string>& value)>> getDetailExpander() const { return std::nullopt; }
};

class ExportableDeviceHistoryItem : public ExportableItem {
private:
  std::string columnName_;
  std::optional<std::string> description_;
public:
  ExportableDeviceHistoryItem(const std::string& columnName, const std::optional<std::string>& description);

  std::string getSourceColumnName() const override { return columnName_; }
  std::optional<std::string> getDescription() const override { return description_; }
  std::string getCaptionPrefix() const override { return "Device history"; }

  std::optional<std::function<void(ExportDataTable&, const std::optional<std::string>&)>> getDetailExpander() const override;
};

class ExportableShortPseudonymItem : public ExportableItem {
private:
  std::string columnName_;
  std::string description_;
  std::optional<unsigned int> visitNumber_;

public:
  explicit ExportableShortPseudonymItem(const pep::ShortPseudonymDefinition& definition);
  std::string getSourceColumnName() const override { return columnName_; }
  std::optional<std::string> getDescription() const override { return description_; }
  std::optional<unsigned int> getVisitNumber() const override { return visitNumber_; }
  std::string getCaptionPrefix() const override { return "Short Pseudonym"; }
};

class ExportableParticipantIdentifierItem : public ExportableItem {
public:
  std::string getSourceColumnName() const override { return "ParticipantIdentifier"; }
  std::string getCaptionPrefix() const override { return "PEP ID"; }
};

class ExportableVisitAssessorItem : public ExportableItem {
private:
  std::string columnName_;
  std::optional<unsigned int> visitNumber_;

public:
  ExportableVisitAssessorItem(const std::string& columnName, const unsigned int visitNumber);
  std::string getSourceColumnName() const override { return columnName_; }
  std::string getCaptionPrefix() const override { return "Assessor"; }
  std::optional<unsigned int> getVisitNumber() const override { return visitNumber_; }
};
