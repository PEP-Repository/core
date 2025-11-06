#pragma once

#include <chrono>
#include <string>

namespace pep {

class ParticipantPersonalia {
private:
  std::string mFirstName;
  std::string mMiddleName;
  std::string mLastName;
  std::string mDateOfBirth;

public:
  ParticipantPersonalia(const std::string& firstName, const std::string& middleName, const std::string& lastName, const std::string& dateOfBirth);

  inline const std::string &getFirstName() const noexcept { return mFirstName; }
  inline const std::string &getMiddleName() const noexcept { return mMiddleName; }
  inline const std::string &getLastName() const noexcept { return mLastName; }
  inline const std::string &getDateOfBirth() const noexcept { return mDateOfBirth; }

  std::string getFullName() const;

  static std::chrono::year_month_day ParseDateOfBirth(const std::string& value);

  std::string toJson() const;
  static ParticipantPersonalia FromJson(const std::string& json);

  bool operator ==(const ParticipantPersonalia& rhs) const;
  inline bool operator !=(const ParticipantPersonalia& rhs) const { return !(*this == rhs); }
};

}
