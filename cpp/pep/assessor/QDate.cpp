#include <pep/assessor/QDate.hpp>

#include <pep/utils/Platform.hpp>

#include <boost/date_time/c_local_time_adjustor.hpp>

namespace {

  boost::date_time::months_of_year MonthIndexToBoost(int index) {
    if ((index < 0) || (index >= 12)) {
      throw std::range_error("Month index must be between 0 and 11 (inclusive)");
    }

    return boost::date_time::months_of_year(boost::date_time::months_of_year::Jan + index);
  }

  int MonthBoostToIndex(boost::date_time::months_of_year month) {
    if ((month < boost::date_time::months_of_year::Jan) || (month > boost::date_time::months_of_year::Dec)) {
      throw std::range_error("Month must be a value between January and December (inclusive)");
    }
    return month - boost::date_time::months_of_year::Jan;
  }

  int MonthBoostToQt(boost::date_time::months_of_year month) {
    return MonthBoostToIndex(month) + 1;
  }

  boost::date_time::months_of_year MonthQtToBoost(int qt) {
    return MonthIndexToBoost(qt - 1);
  }
}

QDateTime MakeLocalQDateTime(QDate date, QTime time) {
#if QT_VERSION >= 0x060500
  return QDateTime(date, time); // Exists since QT 6.5
#else
  return QDateTime(date, time, Qt::LocalTime); // Deprecated since QT 6.9
#endif
}

boost::gregorian::date QDateToGregorian(const QDate& value) {
  if (!value.isValid()) {
    return {};
  }
  namespace bg = boost::gregorian;
  return {
    boost::numeric_cast<bg::greg_year::value_type>(value.year()),
    MonthQtToBoost(value.month()),
    boost::numeric_cast<bg::greg_day::value_type>(value.day())
  };
}

QDate GregorianToQDate(const boost::gregorian::date& gregorian) {
  if (gregorian.is_special()) {
    throw std::runtime_error("Can only convert a concrete date value");
  }
  return QDate(gregorian.year(), MonthBoostToQt(gregorian.month().as_enum()), gregorian.day());
}


//TODO use value.toSecsSinceEpoch() -> std::seconds -> system_clock::time_point -> system_clock::to_time_t
time_t LocalQDateTimeToTimeT(const QDateTime& value) {
  if (value.timeSpec() != Qt::LocalTime) {
    throw std::runtime_error("DateTime can only convert QDateTime instances based on local time spec");
  }

  auto date = value.date();
  auto time = value.time();

  std::tm local{};
  local.tm_year = date.year() - 1900; // tm::tm_year represents "years since 1900": http://www.cplusplus.com/reference/ctime/tm/
  local.tm_mon = date.month() - 1; // 1-based to 0-based
  local.tm_mday = date.day();
  local.tm_hour = time.hour();
  local.tm_min = time.minute();
  local.tm_sec = time.second();
  local.tm_isdst = -1; // "less than zero if the information is not available": http://www.cplusplus.com/reference/ctime/tm/

  auto result = mktime(&local);
  if (result == -1) {
    throw std::runtime_error("Conversion from local QDateTime failed");
  }
  if (local.tm_isdst < 0) {
    throw std::runtime_error("Conversion could not determine DST state for the specified date+time");
  }

  return result;
}

//TODO use system_clock::from_time_t -> system_clock::time_point::time_since_epoch -> std::seconds -> QDateTime::fromSecsSinceEpoch()
QDateTime TimeTToLocalQDateTime(time_t value) {
  std::tm local{};
  if (!localtime_r(&value, &local)) {
    throw std::runtime_error("Failed to convert time");
  }
  if (local.tm_isdst < 0) {
    throw std::runtime_error("Conversion could not determine DST state for the specified timestamp");
  }

  auto date = QDate(
    1900 + local.tm_year, // tm::tm_year represents "years since 1900": http://www.cplusplus.com/reference/ctime/tm/
    1 + local.tm_mon, // 0-based to 1-based
    local.tm_mday);
  auto time = QTime(local.tm_hour, local.tm_min, local.tm_sec);

  return MakeLocalQDateTime(date, time);
}
