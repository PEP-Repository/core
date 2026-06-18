#pragma once

#include <pep/morphing/EnrolledPartyKeys.hpp>
#include <pep/morphing/SystemPublicKeys.hpp>
#include <pep/utils/PropertySerializer.hpp>

#include <stdexcept>

namespace pep {

class UnsupportedEnrollmentSchemeError : public std::runtime_error {
  EnrollmentScheme scheme_{};

public:
  UnsupportedEnrollmentSchemeError(EnrollmentScheme scheme);

  [[nodiscard]] EnrollmentScheme scheme() const noexcept { return scheme_; }
};

template <>
class PropertySerializer<EnrolledPartyKeys> : public PropertySerializerByValue<EnrolledPartyKeys> {
public:
  void write(boost::property_tree::ptree& destination, const EnrolledPartyKeys& value) const override;
  /// \throws UnsupportedEnrollmentSchemeError when inline keys are present for an unsupported EnrollmentScheme
  EnrolledPartyKeys read(const boost::property_tree::ptree& source, const DeserializationContext& context) const override;
};

template <>
class PropertySerializer<SystemPublicKeys> : public PropertySerializerByValue<SystemPublicKeys> {
public:
  void write(boost::property_tree::ptree& destination, const SystemPublicKeys& value) const override;
  SystemPublicKeys read(const boost::property_tree::ptree& source, const DeserializationContext& context) const override;
};

}
