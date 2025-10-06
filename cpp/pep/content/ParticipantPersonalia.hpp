#pragma once

#include <pep/content/Date.hpp>
#include <string>

namespace pep {

struct ParticipantPersonalia {
  std::string firstName;
  std::string middleName;
  std::string lastName;
  std::string dateOfBirth;

  std::string getFullName() const;

  static Date ParseDateOfBirth(const std::string& value);

  std::string toJson() const;
  static ParticipantPersonalia FromJson(const std::string& json);

  [[nodiscard]] auto operator <=>(const ParticipantPersonalia& rhs) const = default;
};

}
