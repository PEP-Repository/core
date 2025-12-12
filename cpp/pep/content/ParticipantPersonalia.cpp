#include <pep/content/ParticipantPersonalia.hpp>

#include <pep/content/Date.hpp>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <regex>

namespace pep {

std::chrono::year_month_day ParticipantPersonalia::ParseDateOfBirth(const std::string& value) {
  auto result = TryParseDdMonthAbbrevYyyyDate(value);
  if (result == std::nullopt) {
    result = TryParseDdMmYyyy(value);
    if (result == std::nullopt) {
      throw std::runtime_error("Value could not be parsed as date of birth");
    }
  }

  return *result;
}

std::string ParticipantPersonalia::getFullName() const {
  std::string result;
  for (auto& part : { firstName, middleName, lastName }) {
    if (!part.empty()) {
      if (!result.empty()) {
        result += " ";
      }
      result += part;
    }
  }
  return result;
}

std::string ParticipantPersonalia::toJson() const {
  boost::property_tree::ptree properties;
  properties.add<std::string>("FirstName", firstName);
  properties.add<std::string>("MiddleName", middleName);
  properties.add<std::string>("LastName", lastName);
  properties.add<std::string>("DoB", dateOfBirth);

  std::ostringstream result;
  boost::property_tree::write_json(result, properties);
  return std::move(result).str();
}

ParticipantPersonalia ParticipantPersonalia::FromJson(const std::string& json) {
  std::istringstream ss(json);
  boost::property_tree::ptree properties;
  boost::property_tree::read_json(ss, properties);

  return ParticipantPersonalia{
    .firstName = properties.get<std::string>("FirstName"),
    .middleName = properties.get<std::string>("MiddleName"),
    .lastName = properties.get<std::string>("LastName"),
    .dateOfBirth = properties.get<std::string>("DoB"),
  };
}

}
