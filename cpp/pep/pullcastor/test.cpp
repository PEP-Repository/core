#include <date/tz.h>
#include <iostream>

int main() {
  date::local_time<std::chrono::milliseconds> datetime;
  std::string input = "2019-02-11 15:36:40.000000";
  std::istringstream datestringstream(input.substr(0, input.find('.')));
  std::string timezone;
  datestringstream >> date::parse("%Y-%m-%d %H:%M:%S", datetime, timezone);
  if(datestringstream.fail()) {
    std::cerr << "Error parsing date" << std::endl;
    throw std::runtime_error("Error parsing date");
  }

  std::cout << "in: " << input << " timezone: " << timezone << " out: " << date::make_zoned(date::locate_zone("Europe/Amsterdam"), datetime).get_sys_time().time_since_epoch().count() << std::endl;
}