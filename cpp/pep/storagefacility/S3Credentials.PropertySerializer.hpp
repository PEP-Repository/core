#pragma once

#include <pep/storagefacility/S3Credentials.hpp>
#include <pep/utils/PropertySerializer.hpp>

// (De)serializer for s3::Credentials, needed to credentials
// from configuration files see utils/PropertySerializer.hpp
// and utils/Configuration.hpp.
namespace pep
{
  template <>
  class PropertySerializer<s3::Credentials>
    : public PropertySerializerByValue<s3::Credentials> {
  public:
    void write(
        boost::property_tree::ptree& destination,
        const s3::Credentials& value) const override;

    s3::Credentials read(
        const boost::property_tree::ptree& source,
        const MultiTypeTransform& transform) const override;
  };
}
