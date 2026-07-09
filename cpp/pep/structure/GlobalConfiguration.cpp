#include <pep/structure/GlobalConfiguration.hpp>
#include <pep/utils/Log.hpp>

#include <boost/algorithm/string/predicate.hpp>

namespace pep {

UserPseudonymFormat::UserPseudonymFormat(const std::string &prefix, size_t length)
  : prefix_(prefix),length_(length) {
  if(prefix_.empty()) {
    throw std::runtime_error("User pseudonym format prefix must be nonempty");
  }
  if(length_ <= 0) {
    throw std::runtime_error("Length of user pseudonym must be positive");
  }
  if (length_ > LocalPseudonym::TextLength()) {
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
  return userPseudonym.erase(0, prefix_.length());
}

std::string UserPseudonymFormat::makeUserPseudonym(const LocalPseudonym& localpseudonym) const {
  return prefix_ + localpseudonym.text().substr(0, length_);
}

bool UserPseudonymFormat::matches(const std::string& input) const
{
  return input.length() == this->getTotalLength() && input.starts_with(prefix_);
}

PseudonymFormat::PseudonymFormat(std::string prefix, size_t digits)
  : prefix_(std::move(prefix)), digits_(digits) {
  if (prefix_.empty()) {
    throw std::runtime_error("Pseudonym format prefix must be nonempty");
  }
  if (digits_ == 0U) {
    throw std::runtime_error("Number of generated pseudonym digits must be a positive number");
  }

  // Case insensitive matching on prefix
  for (auto c : prefix_) {
    char upper = static_cast<char>(toupper(c));
    char lower = static_cast<char>(tolower(c));
    assert(c == upper || c == lower);
    if (upper == lower) {
      regexPattern_ += upper;
    }
    else
    {
      regexPattern_ += std::string{'[', upper, lower, ']'};
    }
  }

  // Require as many digits as configured, plus those for the checksum
  regexPattern_ += "[0-9]{" + std::to_string(*getTotalNumberOfDigits()) + "}";
}

PseudonymFormat::PseudonymFormat(std::string regexPattern)
  : prefix_(), digits_(0U), regexPattern_(std::move(regexPattern)) {
  if (regexPattern_.empty()) {
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
  auto end = shortPseudonyms_.cend();
  auto position = std::find_if(shortPseudonyms_.cbegin(), end, [column](ShortPseudonymDefinition candidate) {
    return column == candidate.getColumn().getFullName();
  });
  if (position == end) {
    return std::nullopt;
  }
  return *position;
}

std::optional<ShortPseudonymDefinition> GlobalConfiguration::getShortPseudonymForValue(const std::string& value) const noexcept {
  // Look up the value in the errata
  auto errataEnd = spErrata_.cend();
  auto erratum = std::find_if(spErrata_.cbegin(), errataEnd, [&value](const ShortPseudonymErratum& candidate) { return candidate.value == value; });
  if (erratum != errataEnd) {
    assert(!erratum->column.empty());
    return this->getShortPseudonym(erratum->column);
  }

  // Look up the value's prefix in the short pseudonym definitions
  auto spsEnd = shortPseudonyms_.cend();
  auto sp = std::find_if(shortPseudonyms_.cbegin(), spsEnd, [&value](const ShortPseudonymDefinition& candidate) {return value.starts_with(candidate.getPrefix()); });
  if (sp != spsEnd) {
    return *sp;
  }

  // Can't find the SP definition corresponding with this value
  return std::nullopt;
}


std::optional<ColumnSpecification> GlobalConfiguration::getColumnSpecification(const std::string& column) const noexcept {
  auto end = columnSpecifications_.cend();
  auto position = std::find_if(columnSpecifications_.cbegin(), end, [column](ColumnSpecification candidate) {
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
  : participantIdentifierFormats_(std::move(participantIdentifierFormats))
  , studyContexts_(std::move(studyContexts))
  , shortPseudonyms_(std::move(shortPseudonyms))
  , userPseudonymFormat_(std::move(userPseudonymFormat))
  , additionalStickers_(std::move(additionalStickers))
  , devices_(std::move(devices))
  , assessors_(std::move(assessors))
  , columnSpecifications_(std::move(columnSpecifications))
  , spErrata_(std::move(spErrata)) {
  if (participantIdentifierFormats_.empty()) {
    throw std::runtime_error("No participant identifier formats specified");
  }
  if (!participantIdentifierFormats_.front().isGenerable()) {
    throw std::runtime_error("First specified participant identifier format must be generable");
  }

  for (auto i = shortPseudonyms_.cbegin(); i != shortPseudonyms_.cend(); ++i) {
    const auto& sp = *i;
    const auto& context = sp.getStudyContext();
    numberOfVisits_[context] = std::max(getNumberOfVisits(context), sp.getColumn().getVisitNumber().value_or(0));

    for (auto j = i + 1; j < shortPseudonyms_.cend(); ++j) {
      if (sp.getColumn().getFullName() == j->getColumn().getFullName()) {
        throw std::runtime_error("Duplicate short pseudonym column name found: " + sp.getColumn().getFullName());
      }
      if (sp.getPrefix().starts_with(j->getPrefix()) || j->getPrefix().starts_with(sp.getPrefix())) {
        throw std::runtime_error("Overlapping short pseudonym prefixes found: " + sp.getPrefix() + " and " + j->getPrefix());
      }
    }
  }

  for (const auto& additional : additionalStickers_) {
    if (ShortPseudonymColumn::Parse(additional.studyContext, additional.column).getVisitNumber() == additional.visit) {
      throw std::runtime_error("Use regular instead of additional sticker specification for short pseudonym " + additional.column);
    }
    if (!getShortPseudonym(additional.column)) {
      throw std::runtime_error("Cannot specify additional stickers for undefined short pseudonym " + additional.column);
    }
    numberOfVisits_[additional.studyContext] = std::max(getNumberOfVisits(additional.studyContext), additional.visit);
  }

  for (const auto& assessor : assessors_) {
    for (const auto& contextId : assessor.studyContexts) {
      try {
        studyContexts_.getById(contextId); // Throws an exception if context not found
      }
      catch (...) {
        throw std::runtime_error("Error finding study context '" + contextId + "', configured for assessor " + std::to_string(assessor.id));
      }
    }
  }

  for (const auto& columnSpec : columnSpecifications_) {
    if(auto sp = columnSpec.getAssociatedShortPseudonymColumn()) {
      if(!getShortPseudonym(*sp)) {
        throw std::runtime_error("Associated short pseudonym column " + *sp + " for column " + columnSpec.getColumn() + " does not exist");
      }
    }
  }

  for (const auto& erratum : spErrata_) {
    assert(!erratum.column.empty());
    if (!getShortPseudonym(erratum.column)) {
      throw std::runtime_error("Short pseudonym erratum column " + erratum.column + " does not exist");
    }
  }
}

std::vector<ShortPseudonymDefinition> GlobalConfiguration::getShortPseudonyms(const std::string& studyContext, const std::optional<uint32_t>& visitNumber) const {
  std::vector<ShortPseudonymDefinition> result;
  auto inserter = std::back_inserter(result);
  std::copy_if(shortPseudonyms_.cbegin(), shortPseudonyms_.cend(), inserter, [studyContext, visitNumber](const ShortPseudonymDefinition& candidate) {return candidate.getStudyContext() == studyContext && candidate.getColumn().getVisitNumber() == visitNumber; });

  for (const auto& entry : additionalStickers_) {
    if (entry.studyContext == studyContext && entry.visit == visitNumber) {
      auto defined = *getShortPseudonym(entry.column);
      result.emplace_back(entry.column, defined.getPrefix(), defined.getLength(), defined.getCastor(),
        entry.stickers, entry.suppressAdditionalStickers, defined.getConfiguredDescription(),
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
  auto i = numberOfVisits_.find(studyContext);
  if (i == numberOfVisits_.cend()) {
    return 0;
  }
  return i->second;
}

}
