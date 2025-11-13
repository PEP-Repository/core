#pragma once

#include <pep/crypto/Timestamp.hpp>

#include <QDate>

namespace pep {
[[nodiscard]] QDateTime MakeLocalQDateTime(const QDate& date, const QTime& time);

[[nodiscard]] QDate QDateFromStd(const std::chrono::year_month_day& date);
[[nodiscard]] std::chrono::year_month_day QDateToStd(const QDate& date);

[[nodiscard]] Timestamp QDateTimeToStdTimestamp(const QDateTime& value);
[[nodiscard]] QDateTime LocalQDateTimeFromStdTimestamp(Timestamp value);
}
