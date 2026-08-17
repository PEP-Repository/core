#include <pep/structure/ShortPseudonyms.hpp>
#include <pep/utils/Mod97.hpp>
#include <pep/utils/Random.hpp>

#include <boost/algorithm/string/predicate.hpp>

#include <random>

namespace {

const char ShortPseudonymSectionDelimiter = '.';
const std::string ShortPseudonymPrefix = "ShortPseudonym";
const std::string ShortPseudonymVisitPrefix = "Visit";
const size_t ShortPseudonymVisitPrefixLength = ShortPseudonymVisitPrefix.size();

const std::string ShortPseudonymPreamble = ShortPseudonymPrefix + ShortPseudonymSectionDelimiter;
const size_t ShortPseudonymPreambleLength = ShortPseudonymPreamble.size();

}

namespace pep {

std::string GenerateShortPseudonym(std::string_view prefix, const std::size_t len) {
  static constexpr std::string_view SpChars = "0123456789";
  std::string pseudonym;
  pseudonym.reserve(prefix.length() + len + 2);
  pseudonym += prefix;

  std::uniform_int_distribution sp_distribution(std::size_t{}, SpChars.size() - 1);
  CryptoUrbg urbg;
  std::generate_n(std::back_inserter(pseudonym), len, [&sp_distribution, &urbg] {
    return SpChars[sp_distribution(urbg)];
  });

  pseudonym += Mod97::ComputeCheckDigits(pseudonym);

  return pseudonym;
}

bool ShortPseudonymIsValid(const std::string& shortPseudonym) {
  return Mod97::Verify(shortPseudonym);
}

ShortPseudonymColumn ShortPseudonymColumn::Parse(const std::string& studyContext, const std::string& column) {
  if (!column.starts_with(ShortPseudonymPreamble)) {
    throw std::runtime_error("Invalid short pseudonym column name");
  }

  ShortPseudonymColumn result;
  auto remaining = column.substr(ShortPseudonymPreambleLength);

  if (!studyContext.empty()) {
    auto prefix = studyContext + ShortPseudonymSectionDelimiter;
    if (!boost::istarts_with(remaining, prefix)) {
      throw std::runtime_error("Invalid short pseudonym column name for study context " + studyContext);
    }
    result.studyContext_ = remaining.substr(0, studyContext.length());
    remaining = remaining.substr(prefix.length());
  }

  if (remaining.starts_with(ShortPseudonymVisitPrefix)) {
    remaining = remaining.substr(ShortPseudonymVisitPrefixLength);
    auto visit_start = remaining.data();
    if (isdigit(*visit_start) == 0) { // Also catches end-of-string
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
    if (*visit_end != ShortPseudonymSectionDelimiter) {
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
  auto result = ShortPseudonymPreamble;
  if (!studyContext_.empty()) {
    result += studyContext_ + ShortPseudonymSectionDelimiter;
  }
  if (visit_) {
    result += ShortPseudonymVisitPrefix + std::to_string(*visit_) + ShortPseudonymSectionDelimiter;
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
