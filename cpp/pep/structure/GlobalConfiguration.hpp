#pragma once

#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/structure/StudyContext.hpp>

#include <cstdint>
#include <unordered_map>
#include <optional>

namespace pep {

class AdditionalStickerDefinition {
 public:
  uint32_t mVisit = 0;
  std::string mColumn;
  uint32_t mStickers = 1;
  bool mSuppressAdditionalStickers = false;
  std::string mStudyContext;
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
  std::string mPrefix;
  size_t mLength;

public:
  UserPseudonymFormat(const std::string& prefix, size_t length);

  const std::string& getPrefix() const noexcept { return mPrefix; }
  size_t getLength() const { return mLength; }
  size_t getTotalLength() const;
  std::string stripPrefix(std::string userPseudonym) const;
  std::string makeUserPseudonym(const LocalPseudonym& localpseudonym) const;
  bool matches(const std::string& input) const;
};

/// Format for participant identifier (PEP ID)
class PseudonymFormat {
private:
  std::string mPrefix;
  size_t mDigits;
  std::string mRegexPattern;

public:
  PseudonymFormat(std::string prefix, size_t digits);
  explicit PseudonymFormat(std::string regexPattern);

  const std::string& getRegexPattern() const noexcept { return mRegexPattern; }

  bool isGenerable() const noexcept { return mDigits > 0U; }
  const std::string& getPrefix() const noexcept { return mPrefix; }
  size_t getNumberOfGeneratedDigits() const { return mDigits; }
  std::optional<size_t> getTotalNumberOfDigits() const; // Only produces a value for generable formats
  std::optional<size_t> getLength() const; // Only produces a value for generable formats
};

/*!
 * \brief Contains some column metadata
 * The column may not necessarily exist yet
 */
class ColumnSpecification {
private:
  std::string mColumn;
  std::optional<std::string> mAssociatedShortPseudonymColumn;
  bool mRequiresDirectory;

public:
  ColumnSpecification(const std::string& column, const std::optional<std::string>& associatedShortPseudonymColumn, const bool requiresDirectory)
    : mColumn(column), mAssociatedShortPseudonymColumn(associatedShortPseudonymColumn), mRequiresDirectory(requiresDirectory) {}

  const std::string& getColumn() const noexcept { return mColumn; }
  const std::optional<std::string>& getAssociatedShortPseudonymColumn() const noexcept { return mAssociatedShortPseudonymColumn; }
  bool getRequiresDirectory() const noexcept { return mRequiresDirectory; }
};

struct ShortPseudonymErratum {
  std::string value;
  std::string column;
};

class GlobalConfiguration {
 private:
  std::vector<PseudonymFormat> mParticipantIdentifierFormats;
  StudyContexts mStudyContexts;
  std::vector<ShortPseudonymDefinition> mShortPseudonyms;
  UserPseudonymFormat mUserPseudonymFormat;
  std::vector<AdditionalStickerDefinition> mAdditionalStickers;
  std::vector<DeviceRegistrationDefinition> mDevices;
  std::unordered_map<std::string, uint32_t> mNumberOfVisits;
  std::vector<AssessorDefinition> mAssessors;
  std::vector<ColumnSpecification> mColumnSpecifications;
  std::vector<ShortPseudonymErratum> mSpErrata;

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

  const StudyContexts& getStudyContexts() const noexcept { return mStudyContexts; }

  std::optional<ShortPseudonymDefinition> getShortPseudonym(const std::string& column) const noexcept;
  std::optional<ShortPseudonymDefinition> getShortPseudonymForValue(const std::string& value) const noexcept;
  std::vector<ShortPseudonymDefinition> getShortPseudonyms(const std::string& studyContext, const std::optional<uint32_t>& visitNumber) const; // 1-based. Specify std::nullopt to get SPs that are bound to no visit

  std::optional<ColumnSpecification> getColumnSpecification(const std::string& column) const noexcept;

  const std::vector<PseudonymFormat>& getParticipantIdentifierFormats() const noexcept { return mParticipantIdentifierFormats; }
  const std::vector<ShortPseudonymDefinition>& getShortPseudonyms() const noexcept { return mShortPseudonyms; }
  const UserPseudonymFormat& getUserPseudonymFormat() const noexcept { return mUserPseudonymFormat; }
  const std::vector<AdditionalStickerDefinition>& getAdditionalStickers() const noexcept { return mAdditionalStickers; }
  const std::vector<DeviceRegistrationDefinition>& getDevices() const noexcept { return mDevices; }
  const std::vector<AssessorDefinition>& getAssessors() const noexcept { return mAssessors; }
  const std::vector<ColumnSpecification>& getColumnSpecifications() const noexcept { return mColumnSpecifications; }
  const std::vector<ShortPseudonymErratum>& getShortPseudonymErrata() const noexcept { return mSpErrata; }

  const PseudonymFormat& getGeneratedParticipantIdentifierFormat() const noexcept { return mParticipantIdentifierFormats.front(); }
  std::vector<std::string> getVisitAssessorColumns(const pep::StudyContext& context) const;
  uint32_t getNumberOfVisits(const std::string& studyContext) const noexcept;
};

}
