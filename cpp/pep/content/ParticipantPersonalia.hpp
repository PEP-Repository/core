#pragma once

#include <chrono>
#include <string>

namespace pep {

class ParticipantPersonalia {
private:
  std::string firstName_;
  std::string middleName_;
  std::string lastName_;
  std::string dateOfBirth_;

public:
  ParticipantPersonalia(std::string firstName, std::string middleName, std::string lastName, std::string dateOfBirth)
    : firstName_(std::move(firstName)),
      middleName_(std::move(middleName)),
      lastName_(std::move(lastName)),
      dateOfBirth_(std::move(dateOfBirth)) {}

  inline const std::string &getFirstName() const noexcept { return firstName_; }
  inline const std::string &getMiddleName() const noexcept { return middleName_; }
  inline const std::string &getLastName() const noexcept { return lastName_; }
  inline const std::string &getDateOfBirth() const noexcept { return dateOfBirth_; }

  std::string getFullName() const;

  static std::chrono::year_month_day ParseDateOfBirth(const std::string& value);

  std::string toJson() const;
  static ParticipantPersonalia FromJson(const std::string& json);

  bool operator ==(const ParticipantPersonalia& rhs) const;
  inline bool operator !=(const ParticipantPersonalia& rhs) const { return !(*this == rhs); }
};

}
