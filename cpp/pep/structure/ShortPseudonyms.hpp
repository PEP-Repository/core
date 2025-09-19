#pragma once

#include <optional>
#include <string>
#include <vector>
#include <memory>

namespace pep {

/*!
  * \brief Generate a short pseudonym with the provided prefix, followed by len characters taken from chars, '-' and two check digits
  *
  * \param prefix Prefix for the short pseudonym to be generated
  * \param len Length of random part of the short pseudonym to be generated
  * \return The generated short pseudonym
  */
std::string GenerateShortPseudonym(const std::string& prefix, const int len);

/*!
  * \brief Verify whether the check digits (last two characters) of the provided short pseudonym are valid.
  *
  * \param shortPseudonym p_shortPseudonym:...
  * \return bool
  */
bool ShortPseudonymIsValid(const std::string& shortPseudonym);

enum CastorStudyType {
  STUDY = 0,
  SURVEY = 1,
  REPEATING_DATA = 2
};

class CastorStorageDefinition {
private:
  CastorStudyType mStudyType;
  std::string mDataColumn;
  std::string mImportStudySlug;
  bool mImmediatePartialData;
  std::string mWeekOffsetDeviceColumn;

public:
  inline CastorStorageDefinition(
    CastorStudyType studyType,
    std::string dataColumn,
    std::string importStudySlug,
    bool immediatePartialData,
    std::string weekOffsetDeviceColumn)
    : mStudyType(studyType),
      mDataColumn(std::move(dataColumn)),
      mImportStudySlug(std::move(importStudySlug)),
      mImmediatePartialData(immediatePartialData),
      mWeekOffsetDeviceColumn(std::move(weekOffsetDeviceColumn)) { }

  inline CastorStudyType getStudyType() const noexcept { return mStudyType; }
  inline const std::string& getDataColumn() const noexcept { return mDataColumn; }
  inline const std::string& getImportStudySlug() const noexcept { return mImportStudySlug; }
  inline bool immediatePartialData() const noexcept { return mImmediatePartialData; }
  inline const std::string& getWeekOffsetDeviceColumn() const noexcept { return mWeekOffsetDeviceColumn; }
};

class CastorShortPseudonymDefinition {
private:
  std::string mStudySlug;
  std::string mSiteAbbreviation;
  std::vector<std::shared_ptr<CastorStorageDefinition>> mStorageDefinitions;

public:
  inline CastorShortPseudonymDefinition(
      std::string studySlug,
      std::string siteAbbreviation,
      std::vector<std::shared_ptr<CastorStorageDefinition>> storageDefinitions)
    : mStudySlug(std::move(studySlug)),
      mSiteAbbreviation(std::move(siteAbbreviation)),
      mStorageDefinitions(std::move(storageDefinitions)) { }

  inline const std::string& getStudySlug() const { return mStudySlug; }
  inline const std::string& getSiteAbbreviation() const { return mSiteAbbreviation; }
  inline const std::vector<std::shared_ptr<CastorStorageDefinition>>& getStorageDefinitions() const { return mStorageDefinitions; }
};

class ShortPseudonymColumn {
private:
  std::string mStudyContext;
  std::optional<uint32_t> mVisit;
  std::string mCoreName;

  ShortPseudonymColumn() noexcept = default;

public:
  static ShortPseudonymColumn Parse(const std::string& studyContext, const std::string& column);

  inline const std::string& getStudyContext() const noexcept { return mStudyContext; }
  inline const std::string& getCoreName() const noexcept { return mCoreName; }
  inline const std::optional<uint32_t>& getVisitNumber() const noexcept { return mVisit; }
  std::string getFullName() const;
};

class ShortPseudonymDefinition {
private:
  ShortPseudonymColumn mColumn;
  std::string mPrefix;
  uint32_t mLength;
  std::optional<CastorShortPseudonymDefinition> mCastor;
  uint32_t mStickers;
  bool mSuppressAdditionalStickers;
  std::string mDescription;
  std::string mStudyContext;

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

  inline const ShortPseudonymColumn& getColumn() const { return mColumn; }

  inline const std::string& getPrefix() const { return mPrefix; }
  inline uint32_t getLength() const { return mLength; }

  inline const std::optional<CastorShortPseudonymDefinition>& getCastor() const { return mCastor; }
  inline uint32_t getStickers() const { return mStickers; }
  inline bool getSuppressAdditionalStickers() const noexcept { return mSuppressAdditionalStickers; }
  inline const std::string& getConfiguredDescription() const { return mDescription; }
  inline const std::string& getStudyContext() const { return mStudyContext; }

  std::string getDescription() const;
};

}
