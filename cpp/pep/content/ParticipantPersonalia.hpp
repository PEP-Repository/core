#pragma once

#include <chrono>
#include <string>

namespace pep {

class ParticipantPersonalia {
private:
  std::string mFirstName;
  std::string mMiddleName;
  std::string mLastName;
  std::string dateOfBirth_;

public:
  ParticipantPersonalia(std::string firstName, std::string middleName, std::string lastName, std::string dateOfBirth)
    : mFirstName(std::move(firstName)),
      mMiddleName(std::move(middleName)),
      mLastName(std::move(lastName)),
      dateOfBirth_(std::move(dateOfBirth)) {}

  inline const std::string &getFirstName() const noexcept { return mFirstName; }
  inline const std::string &getMiddleName() const noexcept { return mMiddleName; }
  inline const std::string &getLastName() const noexcept { return mLastName; }
  inline const std::string &getDateOfBirth() const noexcept { return dateOfBirth_; }

  std::string getFullName() const;

  static std::chrono::year_month_day ParseDateOfBirth(const std::string& value);

  std::string toJson() const;
  static ParticipantPersonalia FromJson(const std::string& json);

  bool operator ==(const ParticipantPersonalia& rhs) const;
  inline bool operator !=(const ParticipantPersonalia& rhs) const { return !(*this == rhs); }
};

}
