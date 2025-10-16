#pragma once

#include <pep/crypto/Timestamp.hpp>

#include <optional>
#include <string>

namespace pep {

[[nodiscard]] std::optional<std::chrono::year_month_day> TryParseDdMonthAbbrevYyyyDate(const std::string& value);
[[nodiscard]] std::optional<std::chrono::year_month_day> TryParseDdMmYyyy(const std::string& value);
[[nodiscard]] std::string ToDdMonthAbbrevYyyyDate(const std::chrono::year_month_day& date);

}
