#include <pep/assessor/QDate.hpp>

#include <pep/crypto/Timestamp.hpp>

QDateTime MakeLocalQDateTime(const QDate& date, const QTime& time) {
#if QT_VERSION >= 0x060500
  return QDateTime(date, time); // Exists since QT 6.5
#else
  return QDateTime(date, time, Qt::LocalTime); // Deprecated since QT 6.9
#endif
}

std::chrono::year_month_day QDateToStd(const QDate& date) {
#if __cpp_lib_chrono >= 201907L
  return date.toStdSysDays(); // Get days since epoch for UTC date
#else
  using namespace std::chrono;
  if (!date.isValid()) { return year{0} / month{0} / day{0}; }
  return year{date.year()} / month{static_cast<unsigned>(date.month())} / day{static_cast<unsigned>(date.day())};
#endif
}

QDate QDateFromStd(const std::chrono::year_month_day& date) {
#if __cpp_lib_chrono >= 201907L
  return QDate(date);
#else
  if (!date.ok()) { return QDate{}; }
  return QDate{
    int{date.year()},
    static_cast<int>(unsigned{date.month()}),
    static_cast<int>(unsigned{date.day()})};
#endif
}


pep::Timestamp QDateTimeToStdTimestamp(const QDateTime& value) {
  return pep::Timestamp(std::chrono::milliseconds{value.toMSecsSinceEpoch()});
}

QDateTime LocalQDateTimeFromStdTimestamp(pep::Timestamp value) {
  // Contrary to QDateTime::fromStdTimePoint, this returns a local time.
  return QDateTime::fromMSecsSinceEpoch(value.ticks_since_epoch<std::chrono::milliseconds>());
}
