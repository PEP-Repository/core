#pragma once

#include <pep/cli/DownloadMetadata.hpp>
#include <pep/utils/PropertySerializer.hpp>

template <>
class pep::PropertySerializer<pep::Timestamp> : public PropertySerializerByValue<pep::Timestamp> {
public:
  pep::Timestamp read(const boost::property_tree::ptree& source, const DeserializationContext& context) const override;
  void write(boost::property_tree::ptree& destination, const pep::Timestamp& value) const override;
};

template <>
class pep::PropertySerializer<pep::ElgamalEncryption> : public PropertySerializerByValue<pep::ElgamalEncryption> {
public:
  pep::ElgamalEncryption read(const boost::property_tree::ptree& source, const DeserializationContext& context) const override;
  void write(boost::property_tree::ptree& destination, const pep::ElgamalEncryption& value) const override;
};

template <>
class pep::PropertySerializer<pep::cli::ParticipantIdentifier> : public PropertySerializerByValue<cli::ParticipantIdentifier> {
public:
  cli::ParticipantIdentifier read(const boost::property_tree::ptree& source, const DeserializationContext& context) const override;
  void write(boost::property_tree::ptree& destination, const cli::ParticipantIdentifier& value) const override;
};

template <>
class pep::PropertySerializer<pep::cli::RecordDescriptor> : public PropertySerializerByValue<cli::RecordDescriptor> {
public:
  cli::RecordDescriptor read(const boost::property_tree::ptree& source, const DeserializationContext& context) const override;
  void write(boost::property_tree::ptree& destination, const cli::RecordDescriptor& value) const override;
};

template <>
class pep::PropertySerializer<pep::cli::RecordState> : public PropertySerializerByValue<cli::RecordState> {
public:
  cli::RecordState read(const boost::property_tree::ptree& source, const DeserializationContext& context) const override;
  void write(boost::property_tree::ptree& destination, const cli::RecordState& value) const override;
};
