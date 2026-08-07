#pragma once

#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/utils/Timestamp.hpp>
#include <pep/utils/XxHasher.hpp>
#include <pep/structure/GlobalConfiguration.hpp>
#include <pep/morphing/Metadata.hpp>
#include <pep/utils/Progress.hpp>
#include <pep/utils/CheckedPath.hpp>

#include <filesystem>
#include <optional>
#include <unordered_map>

namespace pep::cli {

class ParticipantIdentifier {
private:
  PolymorphicPseudonym polymorphic_;
  LocalPseudonym local_;

public:
  ParticipantIdentifier(const PolymorphicPseudonym& polymorphic, const LocalPseudonym& local);

  const PolymorphicPseudonym& getPolymorphicPseudonym() const noexcept { return polymorphic_; }
  const LocalPseudonym& getLocalPseudonym() const noexcept { return local_; }

  bool operator ==(const ParticipantIdentifier& other) const { return local_ == other.local_; }
  bool operator !=(const ParticipantIdentifier& other) const { return !(*this == other); }
};

class RecordDescriptor {
private:
  ParticipantIdentifier participant_;
  std::string column_;
  Timestamp blindingTimestamp_;
  std::optional<Timestamp> payloadBlindingTimestamp_;
  std::map<std::string, MetadataXEntry> extra_;

public:
  RecordDescriptor(const ParticipantIdentifier& participant, const std::string& column, const Timestamp& blindingTimestamp, const std::optional<Timestamp>& payloadBlindingTimestamp = std::nullopt);
  RecordDescriptor(const ParticipantIdentifier& participant, const std::string& column, const Timestamp& blindingTimestamp, const std::map<std::string, MetadataXEntry>& extra, const std::optional<Timestamp>& payloadBlindingTimestamp = std::nullopt);

  const ParticipantIdentifier& getParticipant() const noexcept { return participant_; }
  const std::string& getColumn() const noexcept { return column_; }
  const Timestamp& getBlindingTimestamp() const noexcept { return blindingTimestamp_; }
  const Timestamp& getPayloadBlindingTimestamp() const noexcept;

  CheckedFileName getFileName(bool includingExtension = true) const noexcept;
  const std::map<std::string, MetadataXEntry>& getExtra() const noexcept { return extra_; }

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

  std::shared_ptr<GlobalConfiguration> globalConfig_;
  CheckedPath downloadDirectory_;
  std::shared_ptr<std::unordered_map<CheckedRelativeFilePath, Snapshot>> snapshotsByRelativePath_ = std::make_shared<decltype(snapshotsByRelativePath_)::element_type>(); // Heap-allocated so it can be updated from const methods
  std::shared_ptr<std::unordered_map<RecordDescriptor, CheckedRelativeFilePath>> relativePathsByDescriptor_ = std::make_shared<decltype(relativePathsByDescriptor_)::element_type>(); // Heap-allocated so it can be updated from const methods

  CheckedPath provideDirectory() const;
  CheckedPath provideParticipantDirectory(const LocalPseudonym& localPseudonym) const;
  void ensureFormatUpToDate();

public:
  explicit DownloadMetadata(const std::filesystem::path& downloadDirectory, std::shared_ptr<GlobalConfiguration> globalConfig, const Progress::OnCreation& onCreateProgress = [](std::shared_ptr<const Progress>) {});

  CheckedPath getDirectory() const;
  std::vector<RecordState> getRecords() const;
  std::optional<XxHasher::Hash> getHash(const RecordDescriptor& record) const;
  std::optional<CheckedRelativeFilePath> getRelativePath(const RecordDescriptor& record) const;
  void add(const RecordDescriptor& record, const CheckedFileName& dataFileName, XxHasher::Hash hash);
  bool remove(const RecordDescriptor& record);
};

}


// Specialize std::hash to enable usage of types as keys in std::unordered_map
namespace std {
  template <> struct hash<pep::cli::ParticipantIdentifier> {
    size_t operator()(const pep::cli::ParticipantIdentifier& k) const { return std::hash<pep::LocalPseudonym>()(k.getLocalPseudonym()); }
  };

  template <> struct hash<pep::cli::RecordDescriptor> {
    size_t operator()(const pep::cli::RecordDescriptor& k) const {
      return std::hash<pep::cli::ParticipantIdentifier>()(k.getParticipant())
        ^ std::hash<std::string>()(k.getColumn())
        ^ std::hash<int64_t>()(pep::TicksSinceEpoch<std::chrono::milliseconds>(k.getBlindingTimestamp()));
    }
  };
}
