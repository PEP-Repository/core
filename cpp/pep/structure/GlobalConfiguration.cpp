#include <pep/structure/GlobalConfiguration.hpp>
#include <pep/utils/Log.hpp>

#include <boost/algorithm/string/predicate.hpp>

namespace pep {

UserPseudonymFormat::UserPseudonymFormat(const std::string &prefix, size_t length)
  : mPrefix(prefix),mLength(length) {
  if(mPrefix.empty()) {
    throw std::runtime_error("User pseudonym format prefix must be nonempty");
  }
  if(mLength <= 0) {
    throw std::runtime_error("Length of user pseudonym must be positive");
  }
  if (mLength > LocalPseudonym::TextLength()) {
    throw std::runtime_error("User pseudonym cannot be longer than local pseudonym");
  }
}

bool AssessorDefinition::matchesStudyContext(const StudyContext& context) const {
  if (studyContexts.empty()) {
    return context.isDefault();
  }
  auto contextsEnd = studyContexts.cend();
  return std::find_if(studyContexts.cbegin(), contextsEnd, [id = context.getId()](const std::string& candidate) { return boost::iequals(candidate, id); }) != contextsEnd;
}

size_t UserPseudonymFormat::getTotalLength() const {
  return this->getPrefix().size() + this->getLength();
}

std::string UserPseudonymFormat::stripPrefix(std::string userPseudonym) const {
  assert(this->matches(userPseudonym));
  return userPseudonym.erase(0, mPrefix.length());
}

std::string UserPseudonymFormat::makeUserPseudonym(const LocalPseudonym& localpseudonym) const {
  return mPrefix + localpseudonym.text().substr(0, mLength);
}

bool UserPseudonymFormat::matches(const std::string& input) const
{
  return input.length() == this->getTotalLength() && input.starts_with(mPrefix);
}

PseudonymFormat::PseudonymFormat(std::string prefix, size_t digits)
  : mPrefix(std::move(prefix)), mDigits(digits) {
  if (mPrefix.empty()) {
    throw std::runtime_error("Pseudonym format prefix must be nonempty");
  }
  if (mDigits == 0U) {
    throw std::runtime_error("Number of generated pseudonym digits must be a positive number");
  }

  // Case insensitive matching on prefix
  for (auto c : mPrefix) {
    char upper = static_cast<char>(toupper(c));
    char lower = static_cast<char>(tolower(c));
    assert(c == upper || c == lower);
    if (upper == lower) {
      mRegexPattern += upper;
    }
    else
    {
      mRegexPattern += std::string{'[', upper, lower, ']'};
    }
  }

  // Require as many digits as configured, plus those for the checksum
  mRegexPattern += "[0-9]{" + std::to_string(*getTotalNumberOfDigits()) + "}";
}

PseudonymFormat::PseudonymFormat(std::string regexPattern)
  : mPrefix(), mDigits(0U), mRegexPattern(std::move(regexPattern)) {
  if (mRegexPattern.empty()) {
    throw std::runtime_error("No pattern specified for pseudonym format");
  }
}

std::optional<size_t> PseudonymFormat::getTotalNumberOfDigits() const {
  auto generated = this->getNumberOfGeneratedDigits();
  if (generated == 0U) {
    return std::nullopt;
  }
  return generated + 2; // For the checksum
}

std::optional<size_t> PseudonymFormat::getLength() const {
  auto digits = this->getTotalNumberOfDigits();
  if (!digits.has_value()) {
    return std::nullopt;
  }
  return *digits + this->getPrefix().length();
}

std::optional<ShortPseudonymDefinition> GlobalConfiguration::getShortPseudonym(const std::string& column) const noexcept {
  auto end = mShortPseudonyms.cend();
  auto position = std::find_if(mShortPseudonyms.cbegin(), end, [column](ShortPseudonymDefinition candidate) {
    return column == candidate.getColumn().getFullName();
  });
  if (position == end) {
    return std::nullopt;
  }
  return *position;
}

std::optional<ShortPseudonymDefinition> GlobalConfiguration::getShortPseudonymForValue(const std::string& value) const noexcept {
  // Look up the value in the errata
  auto errataEnd = mSpErrata.cend();
  auto erratum = std::find_if(mSpErrata.cbegin(), errataEnd, [&value](const ShortPseudonymErratum& candidate) { return candidate.value == value; });
  if (erratum != errataEnd) {
    assert(!erratum->column.empty());
    return this->getShortPseudonym(erratum->column);
  }

  // Look up the value's prefix in the short pseudonym definitions
  auto spsEnd = mShortPseudonyms.cend();
  auto sp = std::find_if(mShortPseudonyms.cbegin(), spsEnd, [&value](const ShortPseudonymDefinition& candidate) {return value.starts_with(candidate.getPrefix()); });
  if (sp != spsEnd) {
    return *sp;
  }

  // Can't find the SP definition corresponding with this value
  return std::nullopt;
}


std::optional<ColumnSpecification> GlobalConfiguration::getColumnSpecification(const std::string& column) const noexcept {
  auto end = mColumnSpecifications.cend();
  auto position = std::find_if(mColumnSpecifications.cbegin(), end, [column](ColumnSpecification candidate) {
    return column == candidate.getColumn();
  });
  if (position == end) {
    return std::nullopt;
  }
  return *position;
}

GlobalConfiguration::GlobalConfiguration(
  std::vector<PseudonymFormat> participantIdentifierFormats,
  std::vector<StudyContext> studyContexts,
  std::vector<ShortPseudonymDefinition> shortPseudonyms,
  UserPseudonymFormat userPseudonymFormat,
  std::vector<AdditionalStickerDefinition> additionalStickers,
  std::vector<DeviceRegistrationDefinition> devices,
  std::vector<AssessorDefinition> assessors,
  std::vector<ColumnSpecification> columnSpecifications,
  std::vector<ShortPseudonymErratum> spErrata)
  : mParticipantIdentifierFormats(std::move(participantIdentifierFormats))
  , mStudyContexts(std::move(studyContexts))
  , mShortPseudonyms(std::move(shortPseudonyms))
  , mUserPseudonymFormat(std::move(userPseudonymFormat))
  , mAdditionalStickers(std::move(additionalStickers))
  , mDevices(std::move(devices))
  , mAssessors(std::move(assessors))
  , mColumnSpecifications(std::move(columnSpecifications))
  , mSpErrata(std::move(spErrata)) {
  if (mParticipantIdentifierFormats.empty()) {
    throw std::runtime_error("No participant identifier formats specified");
  }
  if (!mParticipantIdentifierFormats.front().isGenerable()) {
    throw std::runtime_error("First specified participant identifier format must be generable");
  }

  for (auto i = mShortPseudonyms.cbegin(); i != mShortPseudonyms.cend(); ++i) {
    const auto& sp = *i;
    const auto& context = sp.getStudyContext();
    mNumberOfVisits[context] = std::max(getNumberOfVisits(context), sp.getColumn().getVisitNumber().value_or(0));

    for (auto j = i + 1; j < mShortPseudonyms.cend(); ++j) {
      if (sp.getColumn().getFullName() == j->getColumn().getFullName()) {
        throw std::runtime_error("Duplicate short pseudonym column name found: " + sp.getColumn().getFullName());
      }
      if (sp.getPrefix().starts_with(j->getPrefix()) || j->getPrefix().starts_with(sp.getPrefix())) {
        throw std::runtime_error("Overlapping short pseudonym prefixes found: " + sp.getPrefix() + " and " + j->getPrefix());
      }
    }
  }

  for (const auto& additional : mAdditionalStickers) {
    if (ShortPseudonymColumn::Parse(additional.mStudyContext, additional.mColumn).getVisitNumber() == additional.mVisit) {
      throw std::runtime_error("Use regular instead of additional sticker specification for short pseudonym " + additional.mColumn);
    }
    if (!getShortPseudonym(additional.mColumn)) {
      throw std::runtime_error("Cannot specify additional stickers for undefined short pseudonym " + additional.mColumn);
    }
    mNumberOfVisits[additional.mStudyContext] = std::max(getNumberOfVisits(additional.mStudyContext), additional.mVisit);
  }

  for (const auto& assessor : mAssessors) {
    for (const auto& contextId : assessor.studyContexts) {
      try {
        mStudyContexts.getById(contextId); // Throws an exception if context not found
      }
      catch (...) {
        throw std::runtime_error("Error finding study context '" + contextId + "', configured for assessor " + std::to_string(assessor.id));
      }
    }
  }

  for (const auto& columnSpec : mColumnSpecifications) {
    if(auto sp = columnSpec.getAssociatedShortPseudonymColumn()) {
      if(!getShortPseudonym(*sp)) {
        throw std::runtime_error("Associated short pseudonym column " + *sp + " for column " + columnSpec.getColumn() + " does not exist");
      }
    }
  }

  for (const auto& erratum : mSpErrata) {
    assert(!erratum.column.empty());
    if (!getShortPseudonym(erratum.column)) {
      throw std::runtime_error("Short pseudonym erratum column " + erratum.column + " does not exist");
    }
  }
}

std::vector<ShortPseudonymDefinition> GlobalConfiguration::getShortPseudonyms(const std::string& studyContext, const std::optional<uint32_t>& visitNumber) const {
  std::vector<ShortPseudonymDefinition> result;
  auto inserter = std::back_inserter(result);
  std::copy_if(mShortPseudonyms.cbegin(), mShortPseudonyms.cend(), inserter, [studyContext, visitNumber](const ShortPseudonymDefinition& candidate) {return candidate.getStudyContext() == studyContext && candidate.getColumn().getVisitNumber() == visitNumber; });

  for (const auto& entry : mAdditionalStickers) {
    if (entry.mStudyContext == studyContext && entry.mVisit == visitNumber) {
      auto defined = *getShortPseudonym(entry.mColumn);
      result.emplace_back(entry.mColumn, defined.getPrefix(), defined.getLength(), defined.getCastor(),
        entry.mStickers, entry.mSuppressAdditionalStickers, defined.getConfiguredDescription(),
        defined.getStudyContext());
    }
  }

  return result;
}

std::vector<std::string> GlobalConfiguration::getVisitAssessorColumns(const pep::StudyContext& context) const {
  std::vector<std::string> result{};
  auto visits = getNumberOfVisits(context.getIdIfNonDefault());
  result.reserve(visits);
  for (unsigned i = 1; i <= visits; ++i) {
    auto column = context.getAdministeringAssessorColumnName(i);
    result.push_back(std::move(column));
  }
  return result;
}


uint32_t GlobalConfiguration::getNumberOfVisits(const std::string& studyContext) const noexcept {
  auto i = mNumberOfVisits.find(studyContext);
  if (i == mNumberOfVisits.cend()) {
    return 0;
  }
  return i->second;
}

}
