#include <pep/structure/ShortPseudonyms.hpp>
#include <pep/utils/Mod97.hpp>
#include <pep/utils/Random.hpp>

#include <boost/algorithm/string/predicate.hpp>

#include <random>

namespace {

const char SHORT_PSEUDONYM_SECTION_DELIMITER = '.';
const std::string SHORT_PSEUDONYM_PREFIX = "ShortPseudonym";
const std::string SHORT_PSEUDONYM_VISIT_PREFIX = "Visit";
const size_t SHORT_PSEUDONYM_VISIT_PREFIX_LENGTH = SHORT_PSEUDONYM_VISIT_PREFIX.size();

const std::string SHORT_PSEUDONYM_PREAMBLE = SHORT_PSEUDONYM_PREFIX + SHORT_PSEUDONYM_SECTION_DELIMITER;
const size_t SHORT_PSEUDONYM_PREAMBLE_LENGTH = SHORT_PSEUDONYM_PREAMBLE.size();

}

namespace pep {

std::string GenerateShortPseudonym(std::string_view prefix, const std::size_t len) {
  static constexpr std::string_view SP_CHARS = "0123456789";
  std::string pseudonym;
  pseudonym.reserve(prefix.length() + len + 2);
  pseudonym += prefix;

  std::uniform_int_distribution sp_distribution(std::size_t{}, SP_CHARS.size() - 1);
  CryptoUrbg urbg;
  std::generate_n(std::back_inserter(pseudonym), len, [&sp_distribution, &urbg] {
    return SP_CHARS[sp_distribution(urbg)];
  });

  pseudonym += Mod97::ComputeCheckDigits(pseudonym);

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
    result.studyContext_ = remaining.substr(0, studyContext.length());
    remaining = remaining.substr(prefix.length());
  }

  if (remaining.starts_with(SHORT_PSEUDONYM_VISIT_PREFIX)) {
    remaining = remaining.substr(SHORT_PSEUDONYM_VISIT_PREFIX_LENGTH);
    auto visit_start = remaining.data();
    if (!isdigit(*visit_start)) { // Also catches end-of-string
      throw std::runtime_error("Invalid short pseudonym column name: missing visit number");
    }
    char *visit_end{};
    result.visit_ = strtol(remaining.data(), &visit_end, 10);
    if (*result.visit_ <= 0) {
      throw std::runtime_error("Invalid short pseudonym column name");
    }
    if (visit_end == visit_start) {
      throw std::runtime_error("Invalid short pseudonym column name");
    }
    if (*visit_end != SHORT_PSEUDONYM_SECTION_DELIMITER) {
      throw std::runtime_error("Invalid short pseudonym column name");
    }
    result.coreName_ = visit_end + 1;
  }
  else {
    result.coreName_ = std::move(remaining);
  }

  if (result.coreName_.empty()) {
    throw std::runtime_error("Invalid short pseudonym column name");
  }
  return result;
}

std::string ShortPseudonymColumn::getFullName() const {
  auto result = SHORT_PSEUDONYM_PREAMBLE;
  if (!studyContext_.empty()) {
    result += studyContext_ + SHORT_PSEUDONYM_SECTION_DELIMITER;
  }
  if (visit_) {
    result += SHORT_PSEUDONYM_VISIT_PREFIX + std::to_string(*visit_) + SHORT_PSEUDONYM_SECTION_DELIMITER;
  }
  result += coreName_;
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
  : column_(ShortPseudonymColumn::Parse(studyContext, column)),
  prefix_(std::move(prefix)),
  length_(length),
  castor_(std::move(castor)),
  stickers_(stickers),
  suppressAdditionalStickers_(suppressAdditionalStickers),
  description_(std::move(description)),
  studyContext_(std::move(studyContext)) {
}

std::string ShortPseudonymDefinition::getDescription() const {
  std::string result = description_;
  if (result.empty()) { // Use the column's core name if no description was specified
    result = column_.getCoreName();
    auto lastPeriod = result.find_last_of('.');
    if (lastPeriod < result.size()) {
      result = result.substr(lastPeriod + 1);
    }
  }
  return result;
}

}
