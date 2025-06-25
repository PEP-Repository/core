#pragma once

#include <pep/content/Date.hpp>
#include <QDate>

QDate GregorianToQDate(const boost::gregorian::date& gregorian);
boost::gregorian::date QDateToGregorian(const QDate& date);

time_t LocalQDateTimeToTimeT(const QDateTime& local);
QDateTime TimeTToLocalQDateTime(time_t value);
