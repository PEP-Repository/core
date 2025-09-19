#include <pep/content/ParticipantPersonalia.hpp>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <regex>

namespace pep {

ParticipantPersonalia::ParticipantPersonalia(const std::string& firstName, const std::string& middleName, const std::string& lastName, const std::string& dateOfBirth)
: mFirstName(firstName), mMiddleName(middleName), mLastName(lastName), mDateOfBirth(dateOfBirth) {
}

Date ParticipantPersonalia::ParseDateOfBirth(const std::string& value) {
  auto result = Date::TryParseHomebrewFormat(value);
  if (result == std::nullopt) {
    result = Date::TryParseDdMmYyyy(value);
    if (result == std::nullopt) {
      throw std::runtime_error("Value could not be parsed as date of birth");
    }
  }

  return *result;
}

std::string ParticipantPersonalia::getFullName() const {
  std::string result;
  for (auto& part : { mFirstName, mMiddleName, mLastName }) {
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
  properties.add<std::string>("FirstName", getFirstName());
  properties.add<std::string>("MiddleName", getMiddleName());
  properties.add<std::string>("LastName", getLastName());
  properties.add<std::string>("DoB", getDateOfBirth());

  std::stringstream result;
  boost::property_tree::write_json(result, properties);
  return result.str();
}

ParticipantPersonalia ParticipantPersonalia::FromJson(const std::string& json) {
  std::stringstream ss(json);
  boost::property_tree::ptree properties;
  boost::property_tree::read_json(ss, properties);

  return ParticipantPersonalia(
    properties.get<std::string>("FirstName"),
    properties.get<std::string>("MiddleName"),
    properties.get<std::string>("LastName"),
    properties.get<std::string>("DoB"));
}

bool ParticipantPersonalia::operator ==(const ParticipantPersonalia& rhs) const {
  return this->getFirstName() == rhs.getFirstName()
    && this->getMiddleName() == rhs.getMiddleName()
    && this->getLastName() == rhs.getLastName()
    && this->getDateOfBirth() == rhs.getDateOfBirth();
}

}
