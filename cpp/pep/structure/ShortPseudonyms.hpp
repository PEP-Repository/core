#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace pep {

/*!
  * \brief Generate a short pseudonym with the provided prefix, followed by len characters taken from chars, '-' and two check digits
  *
  * \param prefix Prefix for the short pseudonym to be generated
  * \param len Length of random part of the short pseudonym to be generated
  * \return The generated short pseudonym
  */
std::string GenerateShortPseudonym(std::string_view prefix, std::size_t len);

/*!
  * \brief Verify whether the check digits (last two characters) of the provided short pseudonym are valid.
  *
  * \param shortPseudonym p_shortPseudonym:...
  * \return bool
  */
bool ShortPseudonymIsValid(const std::string& shortPseudonym);

enum class CastorStudyType {
  Crf = 0,
  Survey = 1,
  RepeatingData = 2
};

class CastorStorageDefinition {
private:
  CastorStudyType studyType_;
  std::string dataColumn_;
  std::string importStudySlug_;
  bool immediatePartialData_;
  std::string weekOffsetDeviceColumn_;

public:
  inline CastorStorageDefinition(
    CastorStudyType studyType,
    std::string dataColumn,
    std::string importStudySlug,
    bool immediatePartialData,
    std::string weekOffsetDeviceColumn)
    : studyType_(studyType),
      dataColumn_(std::move(dataColumn)),
      importStudySlug_(std::move(importStudySlug)),
      immediatePartialData_(immediatePartialData),
      weekOffsetDeviceColumn_(std::move(weekOffsetDeviceColumn)) { }

  inline CastorStudyType getStudyType() const noexcept { return studyType_; }
  inline const std::string& getDataColumn() const noexcept { return dataColumn_; }
  inline const std::string& getImportStudySlug() const noexcept { return importStudySlug_; }
  inline bool immediatePartialData() const noexcept { return immediatePartialData_; }
  inline const std::string& getWeekOffsetDeviceColumn() const noexcept { return weekOffsetDeviceColumn_; }
};

class CastorShortPseudonymDefinition {
private:
  std::string studySlug_;
  std::string siteAbbreviation_;
  std::vector<std::shared_ptr<CastorStorageDefinition>> storageDefinitions_;

public:
  inline CastorShortPseudonymDefinition(
      std::string studySlug,
      std::string siteAbbreviation,
      std::vector<std::shared_ptr<CastorStorageDefinition>> storageDefinitions)
    : studySlug_(std::move(studySlug)),
      siteAbbreviation_(std::move(siteAbbreviation)),
      storageDefinitions_(std::move(storageDefinitions)) { }

  inline const std::string& getStudySlug() const { return studySlug_; }
  inline const std::string& getSiteAbbreviation() const { return siteAbbreviation_; }
  inline const std::vector<std::shared_ptr<CastorStorageDefinition>>& getStorageDefinitions() const { return storageDefinitions_; }
};

class ShortPseudonymColumn {
private:
  std::string studyContext_;
  std::optional<uint32_t> visit_;
  std::string coreName_;

  ShortPseudonymColumn() noexcept = default;

public:
  static ShortPseudonymColumn Parse(const std::string& studyContext, const std::string& column);

  inline const std::string& getStudyContext() const noexcept { return studyContext_; }
  inline const std::string& getCoreName() const noexcept { return coreName_; }
  inline const std::optional<uint32_t>& getVisitNumber() const noexcept { return visit_; }
  std::string getFullName() const;
};

class ShortPseudonymDefinition {
private:
  ShortPseudonymColumn column_;
  std::string prefix_;
  uint32_t length_;
  std::optional<CastorShortPseudonymDefinition> castor_;
  uint32_t stickers_;
  bool suppressAdditionalStickers_;
  std::string description_;
  std::string studyContext_;

public:
  ShortPseudonymDefinition(
    std::string column,
    std::string prefix,
    uint32_t length,
    std::optional<CastorShortPseudonymDefinition> castor,
    uint32_t stickers,
    bool suppressAdditionalStickers,
    std::string description,
    std::string studyContext);

  inline const ShortPseudonymColumn& getColumn() const { return column_; }

  inline const std::string& getPrefix() const { return prefix_; }
  inline uint32_t getLength() const { return length_; }

  inline const std::optional<CastorShortPseudonymDefinition>& getCastor() const { return castor_; }
  inline uint32_t getStickers() const { return stickers_; }
  inline bool getSuppressAdditionalStickers() const noexcept { return suppressAdditionalStickers_; }
  inline const std::string& getConfiguredDescription() const { return description_; }
  inline const std::string& getStudyContext() const { return studyContext_; }

  std::string getDescription() const;
};

}
