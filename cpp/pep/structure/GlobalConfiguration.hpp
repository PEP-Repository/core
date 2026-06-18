#pragma once

#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/structure/StudyContext.hpp>

#include <cstdint>
#include <unordered_map>
#include <optional>

namespace pep {

class AdditionalStickerDefinition {
 public:
  uint32_t visit_ = 0;
  std::string column_;
  uint32_t stickers_ = 1;
  bool suppressAdditionalStickers_ = false;
  std::string studyContext_;
};

struct DeviceRegistrationDefinition {
  std::string studyContext;
  std::string columnName;
  std::string serialNumberFormat;
  std::string description;
  std::string tooltip;
  std::string placeholder;
};

struct AssessorDefinition {
  unsigned id{};
  std::string name;
  std::vector<std::string> studyContexts;

  bool matchesStudyContext(const StudyContext& context) const;
};

/// Format for participant alias (shortened local pseudonym)
class UserPseudonymFormat {
private:
  std::string prefix_;
  size_t length_;

public:
  UserPseudonymFormat(const std::string& prefix, size_t length);

  const std::string& getPrefix() const noexcept { return prefix_; }
  size_t getLength() const { return length_; }
  size_t getTotalLength() const;
  std::string stripPrefix(std::string userPseudonym) const;
  std::string makeUserPseudonym(const LocalPseudonym& localpseudonym) const;
  bool matches(const std::string& input) const;
};

/// Format for participant identifier (PEP ID)
class PseudonymFormat {
private:
  std::string prefix_;
  size_t digits_;
  std::string regexPattern_;

public:
  PseudonymFormat(std::string prefix, size_t digits);
  explicit PseudonymFormat(std::string regexPattern);

  const std::string& getRegexPattern() const noexcept { return regexPattern_; }

  bool isGenerable() const noexcept { return digits_ > 0U; }
  const std::string& getPrefix() const noexcept { return prefix_; }
  size_t getNumberOfGeneratedDigits() const { return digits_; }
  std::optional<size_t> getTotalNumberOfDigits() const; // Only produces a value for generable formats
  std::optional<size_t> getLength() const; // Only produces a value for generable formats
};

/*!
 * \brief Contains some column metadata
 * The column may not necessarily exist yet
 */
class ColumnSpecification {
private:
  std::string column_;
  std::optional<std::string> associatedShortPseudonymColumn_;
  bool requiresDirectory_;

public:
  ColumnSpecification(const std::string& column, const std::optional<std::string>& associatedShortPseudonymColumn, const bool requiresDirectory)
    : column_(column), associatedShortPseudonymColumn_(associatedShortPseudonymColumn), requiresDirectory_(requiresDirectory) {}

  const std::string& getColumn() const noexcept { return column_; }
  const std::optional<std::string>& getAssociatedShortPseudonymColumn() const noexcept { return associatedShortPseudonymColumn_; }
  bool getRequiresDirectory() const noexcept { return requiresDirectory_; }
};

struct ShortPseudonymErratum {
  std::string value;
  std::string column;
};

class GlobalConfiguration {
 private:
  std::vector<PseudonymFormat> participantIdentifierFormats_;
  StudyContexts studyContexts_;
  std::vector<ShortPseudonymDefinition> shortPseudonyms_;
  UserPseudonymFormat userPseudonymFormat_;
  std::vector<AdditionalStickerDefinition> additionalStickers_;
  std::vector<DeviceRegistrationDefinition> devices_;
  std::unordered_map<std::string, uint32_t> numberOfVisits_;
  std::vector<AssessorDefinition> assessors_;
  std::vector<ColumnSpecification> columnSpecifications_;
  std::vector<ShortPseudonymErratum> spErrata_;

 public:
  GlobalConfiguration(
    std::vector<PseudonymFormat> participantIdentifierFormats,
    std::vector<StudyContext> studyContexts,
    std::vector<ShortPseudonymDefinition> shortPseudonyms,
    UserPseudonymFormat userPseudonymFormat,
    std::vector<AdditionalStickerDefinition> additionalStickers,
    std::vector<DeviceRegistrationDefinition> devices,
    std::vector<AssessorDefinition> assessors,
    std::vector<ColumnSpecification> columnSpecifications,
    std::vector<ShortPseudonymErratum> spErrata
  );

  const StudyContexts& getStudyContexts() const noexcept { return studyContexts_; }

  std::optional<ShortPseudonymDefinition> getShortPseudonym(const std::string& column) const noexcept;
  std::optional<ShortPseudonymDefinition> getShortPseudonymForValue(const std::string& value) const noexcept;
  std::vector<ShortPseudonymDefinition> getShortPseudonyms(const std::string& studyContext, const std::optional<uint32_t>& visitNumber) const; // 1-based. Specify std::nullopt to get SPs that are bound to no visit

  std::optional<ColumnSpecification> getColumnSpecification(const std::string& column) const noexcept;

  const std::vector<PseudonymFormat>& getParticipantIdentifierFormats() const noexcept { return participantIdentifierFormats_; }
  const std::vector<ShortPseudonymDefinition>& getShortPseudonyms() const noexcept { return shortPseudonyms_; }
  const UserPseudonymFormat& getUserPseudonymFormat() const noexcept { return userPseudonymFormat_; }
  const std::vector<AdditionalStickerDefinition>& getAdditionalStickers() const noexcept { return additionalStickers_; }
  const std::vector<DeviceRegistrationDefinition>& getDevices() const noexcept { return devices_; }
  const std::vector<AssessorDefinition>& getAssessors() const noexcept { return assessors_; }
  const std::vector<ColumnSpecification>& getColumnSpecifications() const noexcept { return columnSpecifications_; }
  const std::vector<ShortPseudonymErratum>& getShortPseudonymErrata() const noexcept { return spErrata_; }

  const PseudonymFormat& getGeneratedParticipantIdentifierFormat() const noexcept { return participantIdentifierFormats_.front(); }
  std::vector<std::string> getVisitAssessorColumns(const pep::StudyContext& context) const;
  uint32_t getNumberOfVisits(const std::string& studyContext) const noexcept;
};

}
