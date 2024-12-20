#pragma once

#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/utils/PropertySerializer.hpp>
#include <pep/crypto/Timestamp.hpp>
#include <pep/utils/XxHasher.hpp>
#include <pep/structure/GlobalConfiguration.hpp>
#include <pep/morphing/Metadata.hpp>

#include <optional>
#include <unordered_map>

namespace pep {

class ParticipantIdentifier {
private:
  PolymorphicPseudonym mPolymorphic;
  LocalPseudonym mLocal;

public:
  ParticipantIdentifier(const PolymorphicPseudonym& polymorphic, const LocalPseudonym& local);

  const PolymorphicPseudonym& getPolymorphicPseudonym() const noexcept { return mPolymorphic; }
  const LocalPseudonym& getLocalPseudonym() const noexcept { return mLocal; }

  bool operator ==(const ParticipantIdentifier& other) const { return mLocal == other.mLocal; }
  bool operator !=(const ParticipantIdentifier& other) const { return !(*this == other); }
};

class RecordDescriptor {
private:
  ParticipantIdentifier mParticipant;
  std::string mColumn;
  Timestamp mBlindingTimestamp;
  std::optional<Timestamp> mPayloadBlindingTimestamp;
  std::map<std::string, MetadataXEntry> mExtra;

public:
  RecordDescriptor(const ParticipantIdentifier& participant, const std::string& column, const Timestamp& blindingTimestamp, const std::optional<Timestamp>& payloadBlindingTimestamp = std::nullopt);
  RecordDescriptor(const ParticipantIdentifier& participant, const std::string& column, const Timestamp& blindingTimestamp, const std::map<std::string, MetadataXEntry>& extra, const std::optional<Timestamp>& payloadBlindingTimestamp = std::nullopt);

  const ParticipantIdentifier& getParticipant() const noexcept { return mParticipant; }
  const std::string& getColumn() const noexcept { return mColumn; }
  const Timestamp& getBlindingTimestamp() const noexcept { return mBlindingTimestamp; }
  const Timestamp& getPayloadBlindingTimestamp() const noexcept;

  std::string getFileName(bool includingExtension = true) const noexcept;
  const std::map<std::string, MetadataXEntry>& getExtra() const noexcept { return mExtra; }

  bool operator ==(const RecordDescriptor& other) const;
};

struct RecordState {
  RecordDescriptor descriptor;
  std::optional<XxHasher::Hash> hash;
};

class DownloadMetadata {
public:
  static std::string GetFilenamePrefix();
  static std::string GetFilenameExtension();
  static std::string GetDirectoryName();

private:
  // Improve performance by caching values related to pristine state. See #1030.
  struct Snapshot {
    std::string serialized;
    RecordState record;
  };

  std::shared_ptr<GlobalConfiguration> mGlobalConfig;
  std::filesystem::path mDownloadDirectory;
  std::shared_ptr<std::unordered_map<std::string, Snapshot>> mSnapshotsByRelativePath = std::make_shared<std::unordered_map<std::string, Snapshot>>(); // Heap-allocated so it can be updated from const methods
  std::shared_ptr<std::unordered_map<RecordDescriptor, std::string>> mRelativePathsByDescriptor = std::make_shared<std::unordered_map<RecordDescriptor, std::string>>(); // Heap-allocated so it can be updated from const methods

  std::filesystem::path provideDirectory() const;
  std::filesystem::path provideParticipantDirectory(const LocalPseudonym& localPseudonym) const;
  void ensureFormatUpToDate();

public:
  explicit DownloadMetadata(const std::filesystem::path& downloadDirectory, std::shared_ptr<GlobalConfiguration> globalConfig);

  std::filesystem::path getDirectory() const;
  std::vector<RecordState> getRecords() const;
  std::optional<XxHasher::Hash> getHash(const RecordDescriptor& record) const;
  std::optional<std::filesystem::path> getRelativePath(const RecordDescriptor& record) const;
  void add(const RecordDescriptor& record, const std::string& dataFileName, XxHasher::Hash hash);
  bool remove(const RecordDescriptor& record);
};

template <>
class PropertySerializer<pep::Timestamp> : public PropertySerializerByValue<pep::Timestamp> {
public:
  pep::Timestamp read(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const override;
  void write(boost::property_tree::ptree& destination, const pep::Timestamp& value) const override;
};

template <>
class PropertySerializer<pep::ElgamalEncryption> : public PropertySerializerByValue<pep::ElgamalEncryption> {
public:
  pep::ElgamalEncryption read(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const override;
  void write(boost::property_tree::ptree& destination, const pep::ElgamalEncryption& value) const override;
};

template <>
class PropertySerializer<ParticipantIdentifier> : public PropertySerializerByValue<ParticipantIdentifier> {
public:
  pep::ParticipantIdentifier read(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const override;
  void write(boost::property_tree::ptree& destination, const pep::ParticipantIdentifier& value) const override;
};

template <>
class PropertySerializer<RecordDescriptor> : public PropertySerializerByValue<RecordDescriptor> {
public:
  RecordDescriptor read(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const override;
  void write(boost::property_tree::ptree& destination, const RecordDescriptor& value) const override;
};

template <>
class PropertySerializer<RecordState> : public PropertySerializerByValue<RecordState> {
public:
  RecordState read(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const override;
  void write(boost::property_tree::ptree& destination, const RecordState& value) const override;
};

}

// Specialize std::hash to enable usage of types as keys in std::unordered_map
namespace std {
  template <> struct hash<pep::ParticipantIdentifier> {
    size_t operator()(const pep::ParticipantIdentifier& k) const { return std::hash<pep::LocalPseudonym>()(k.getLocalPseudonym()); }
  };

  template <> struct hash<pep::RecordDescriptor> {
    size_t operator()(const pep::RecordDescriptor& k) const {
      return std::hash<pep::ParticipantIdentifier>()(k.getParticipant())
        ^ std::hash<std::string>()(k.getColumn())
        ^ std::hash<int64_t>()(k.getBlindingTimestamp().getTime());
    }
  };
}