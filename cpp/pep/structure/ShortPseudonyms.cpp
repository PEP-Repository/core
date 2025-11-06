#include <pep/structure/ShortPseudonyms.hpp>
#include <pep/utils/Mod97.hpp>

#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace {

const char SHORT_PSEUDONYM_SECTION_DELIMITER = '.';
const std::string SHORT_PSEUDONYM_PREFIX = "ShortPseudonym";
const std::string SHORT_PSEUDONYM_VISIT_PREFIX = "Visit";
const size_t SHORT_PSEUDONYM_VISIT_PREFIX_LENGTH = SHORT_PSEUDONYM_VISIT_PREFIX.size();

const std::string SHORT_PSEUDONYM_PREAMBLE = SHORT_PSEUDONYM_PREFIX + SHORT_PSEUDONYM_SECTION_DELIMITER;
const size_t SHORT_PSEUDONYM_PREAMBLE_LENGTH = SHORT_PSEUDONYM_PREAMBLE.size();

}

namespace pep {

std::string GenerateShortPseudonym(const std::string& prefix, const int len) {
  constexpr std::string_view SP_CHARS = "0123456789";

  static boost::random::random_device sp_rng;
  static boost::random::uniform_int_distribution<> sp_distribution = boost::random::uniform_int_distribution<>(
    0, static_cast<int>(SP_CHARS.size() - 1));

  std::string pseudonym{prefix};
  pseudonym.reserve(pseudonym.length() + static_cast<unsigned int>(len) + 2);

  for(int i = 0; i < len; ++i) {
    pseudonym += SP_CHARS[static_cast<size_t>(sp_distribution(sp_rng))];
  }

  std::string checkDigits = Mod97::ComputeCheckDigits(pseudonym);
  pseudonym += checkDigits;

  return pseudonym;
}

bool ShortPseudonymIsValid(const std::string& shortPseudonym) {
  return Mod97::Verify(shortPseudonym);
}

ShortPseudonymColumn ShortPseudonymColumn::Parse(const std::string& studyContext, const std::string& column) {
  if (!column.starts_with(SHORT_PSEUDONYM_PREAMBLE)) {
    throw std::runtime_error("Invalid short pseudonym column name");
  }

  ShortPseudonymColumn result;
  auto remaining = column.substr(SHORT_PSEUDONYM_PREAMBLE_LENGTH);

  if (!studyContext.empty()) {
    auto prefix = studyContext + SHORT_PSEUDONYM_SECTION_DELIMITER;
    if (!boost::istarts_with(remaining, prefix)) {
      throw std::runtime_error("Invalid short pseudonym column name for study context " + studyContext);
    }
    result.mStudyContext = remaining.substr(0, studyContext.length());
    remaining = remaining.substr(prefix.length());
  }

  if (remaining.starts_with(SHORT_PSEUDONYM_VISIT_PREFIX)) {
    remaining = remaining.substr(SHORT_PSEUDONYM_VISIT_PREFIX_LENGTH);
    auto visit_start = remaining.data();
    if (!isdigit(*visit_start)) { // Also catches end-of-string
      throw std::runtime_error("Invalid short pseudonym column name: missing visit number");
    }
    char *visit_end{};
    result.mVisit = strtol(remaining.data(), &visit_end, 10);
    if (*result.mVisit <= 0) {
      throw std::runtime_error("Invalid short pseudonym column name");
    }
    if (visit_end == visit_start) {
      throw std::runtime_error("Invalid short pseudonym column name");
    }
    if (*visit_end != SHORT_PSEUDONYM_SECTION_DELIMITER) {
      throw std::runtime_error("Invalid short pseudonym column name");
    }
    result.mCoreName = visit_end + 1;
  }
  else {
    result.mCoreName = std::move(remaining);
  }

  if (result.mCoreName.empty()) {
    throw std::runtime_error("Invalid short pseudonym column name");
  }
  return result;
}

std::string ShortPseudonymColumn::getFullName() const {
  auto result = SHORT_PSEUDONYM_PREAMBLE;
  if (!mStudyContext.empty()) {
    result += mStudyContext + SHORT_PSEUDONYM_SECTION_DELIMITER;
  }
  if (mVisit) {
    result += SHORT_PSEUDONYM_VISIT_PREFIX + std::to_string(*mVisit) + SHORT_PSEUDONYM_SECTION_DELIMITER;
  }
  result += mCoreName;
  return result;
}

ShortPseudonymDefinition::ShortPseudonymDefinition(
  std::string column,
  std::string prefix,
  uint32_t length,
  std::optional<CastorShortPseudonymDefinition> castor,
  uint32_t stickers,
  bool suppressAdditionalStickers,
  std::string description,
  std::string studyContext)
  : mColumn(ShortPseudonymColumn::Parse(studyContext, column)),
  mPrefix(std::move(prefix)),
  mLength(length),
  mCastor(std::move(castor)),
  mStickers(stickers),
  mSuppressAdditionalStickers(suppressAdditionalStickers),
  mDescription(std::move(description)),
  mStudyContext(std::move(studyContext)) {
}

std::string ShortPseudonymDefinition::getDescription() const {
  std::string result = mDescription;
  if (result.empty()) { // Use the column's core name if no description was specified
    result = mColumn.getCoreName();
    auto lastPeriod = result.find_last_of('.');
    if (lastPeriod < result.size()) {
      result = result.substr(lastPeriod + 1);
    }
  }
  return result;
}

}
