#pragma once

#include <QDate>

// Forward declaration
namespace pep { class Timestamp; }

[[nodiscard]] QDateTime MakeLocalQDateTime(const QDate& date, const QTime& time);

[[nodiscard]] QDate QDateFromStd(const std::chrono::year_month_day& date);
[[nodiscard]] std::chrono::year_month_day QDateToStd(const QDate& date);

[[nodiscard]] pep::Timestamp QDateTimeToStdTimestamp(const QDateTime& value);
[[nodiscard]] QDateTime LocalQDateTimeFromStdTimestamp(pep::Timestamp value);
