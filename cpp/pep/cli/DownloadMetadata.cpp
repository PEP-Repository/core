#include <pep/cli/DownloadMetadata.PropertySerializers.hpp>

#include <pep/utils/File.hpp>
#include <pep/utils/Log.hpp>
#include <filesystem>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/property_tree/json_parser.hpp>

#ifdef _WIN32
#include <Windows.h>
#endif

using namespace pep;
using namespace pep::cli;

namespace {

std::string GetMetaFilenameCore() {
  return "pepData";
}

const std::string LOG_TAG = "Download metadata";

std::string DataFileNameToMetaFileName(const std::string& dataFileName) {
  return DownloadMetadata::GetFilenamePrefix() + dataFileName + DownloadMetadata::GetFilenameExtension();
}

}

std::string DownloadMetadata::GetFilenamePrefix() {
  return GetMetaFilenameCore() + ".";
}

std::string DownloadMetadata::GetFilenameExtension() {
  return ".json";
}

std::string DownloadMetadata::GetDirectoryName() {
  return "." + GetMetaFilenameCore();
}

namespace {

const std::string PRISTINE_STATE_FILENAME = DownloadMetadata::GetFilenamePrefix() + "pristine" + DownloadMetadata::GetFilenameExtension();
const std::string LEGACY_PARTICIPANT_META_FILENAME = DownloadMetadata::GetFilenamePrefix() + "participant" + DownloadMetadata::GetFilenameExtension();

std::vector<std::filesystem::path> GetLegacyParticipantMetaFilePaths(const std::filesystem::path& downloadDirectory) {
  std::vector<std::filesystem::path> result;
  for (std::filesystem::directory_iterator i(downloadDirectory); i != std::filesystem::directory_iterator(); ++i) {
    if (std::filesystem::is_directory(i->path())) {
      auto filename = i->path() / LEGACY_PARTICIPANT_META_FILENAME;
      if (std::filesystem::exists(filename)) {
        result.push_back(filename);
      }
    }
  }
  return result;
}

}

ParticipantIdentifier::ParticipantIdentifier(const PolymorphicPseudonym& polymorphic, const LocalPseudonym& local)
  : mPolymorphic(polymorphic), mLocal(local) {
  if (mPolymorphic == PolymorphicPseudonym{}) {
    throw std::runtime_error("Participant identifier requires a nonempty polymorphic pseudonym");
  }
  if (mLocal == LocalPseudonym{}) {
    throw std::runtime_error("Participant identifier requires a nonempty local pseudonym");
  }
}

RecordDescriptor::RecordDescriptor(const ParticipantIdentifier& participant, const std::string& column, const Timestamp& blindingTimestamp, const std::optional<Timestamp>& payloadBlindingTimestamp)
  : mParticipant(participant), mColumn(column), mBlindingTimestamp(blindingTimestamp), mPayloadBlindingTimestamp(payloadBlindingTimestamp) {
}

RecordDescriptor::RecordDescriptor(const ParticipantIdentifier& participant, const std::string& column, const Timestamp& blindingTimestamp, const std::map<std::string, MetadataXEntry>& extra, const std::optional<Timestamp>& payloadBlindingTimestamp)
  : mParticipant(participant), mColumn(column), mBlindingTimestamp(blindingTimestamp), mPayloadBlindingTimestamp(payloadBlindingTimestamp), mExtra(extra) {
}

const Timestamp& RecordDescriptor::getPayloadBlindingTimestamp() const noexcept {
  if (mPayloadBlindingTimestamp.has_value()) {
    return *mPayloadBlindingTimestamp;
  }
  return mBlindingTimestamp;
}

std::string RecordDescriptor::getFileName(bool includingExtension) const noexcept {
  const auto& column = this->getColumn();

  if (includingExtension) {
    auto found = this->getExtra().find("fileExtension");
    if (found != this->getExtra().end()) {
      return column + found->second.plaintext();
    }
  }

  return column;
}

bool RecordDescriptor::operator==(const RecordDescriptor& other) const {
  return (mParticipant == other.mParticipant) && (mColumn == other.mColumn) && (mBlindingTimestamp == other.mBlindingTimestamp);
}

std::filesystem::path DownloadMetadata::provideDirectory() const {
  auto result = getDirectory();
  if (std::filesystem::create_directories(result)) {
#ifdef _WIN32
    ::SetFileAttributesA(result.string().c_str(), FILE_ATTRIBUTE_HIDDEN);
#endif
  }
  return result;
}

std::filesystem::path DownloadMetadata::provideParticipantDirectory(const LocalPseudonym& localPseudonym) const {
  auto result = provideDirectory() / localPseudonym.text();
  if(std::filesystem::is_directory(result)) {
    return result;
  }
  result = provideDirectory() / mGlobalConfig->getUserPseudonymFormat().makeUserPseudonym(localPseudonym);
  std::filesystem::create_directories(result);
  return result;
}

void DownloadMetadata::ensureFormatUpToDate() {
  /* This method converts the old metadata format to the current one. It does
   * so iteratively, i.e. discarding old files as it ploughs along.
   * It would be better if this were done atomically but since this
   * routine is (expected to be) executed only once (by Verily, who are
   * the only ones that have a download directory), we'll give them upgrade
   * instructions rather than spending lots of time to make this method
   * foolproof.
   */

  auto legacyPristineFile = mDownloadDirectory / PRISTINE_STATE_FILENAME;
  if (std::filesystem::exists(legacyPristineFile)) {
    LOG(LOG_TAG, warning) << "Upgrading legacy download directory format.";
    boost::property_tree::ptree stateProperties;
    boost::property_tree::read_json(legacyPristineFile.string(), stateProperties);
    auto states = DeserializeProperties<std::vector<RecordState>>(stateProperties, "records", MultiTypeTransform());

    auto participantMetaFiles = GetLegacyParticipantMetaFilePaths(mDownloadDirectory);
    for (const auto& participantFile : participantMetaFiles) {
      boost::property_tree::ptree participantProperties;
      boost::property_tree::read_json(participantFile.string(), participantProperties);

      auto local = LocalPseudonym::FromText(participantFile.parent_path().filename().string());
      auto value = participantProperties.get<std::string>("participant");
      auto pp = PolymorphicPseudonym::FromText(value);
      auto id = ParticipantIdentifier(pp, local);

      auto filesProperties = participantProperties.get_child_optional("files");
      if (filesProperties) {
        for (const auto& fileProperties : *filesProperties) {
          assert(fileProperties.first.empty());
          auto filename = fileProperties.second.get<std::string>("filename");
          std::vector<std::string> parts;
          boost::algorithm::split(parts, filename, boost::algorithm::is_space());
          assert(parts.size() > 0);
          const auto& column = parts[0];
          auto timestamp = DeserializeProperties<Timestamp>(fileProperties.second, "timestamp", MultiTypeTransform());

          RecordDescriptor descriptor(id, column, timestamp);

          auto position = std::find_if(states.cbegin(), states.cend(), [&descriptor](const RecordState& candidate) {return candidate.descriptor == descriptor; });
          if (position == states.cend()) {
            throw std::runtime_error("Could not find pristine state for participant " + local.text()
                + ", column " + column
                + ", timestamp " + std::to_string(timestamp.ticks_since_epoch<std::chrono::milliseconds>()));
          }

          if (position->hash) {
            add(descriptor, filename, *position->hash);
          }
          states.erase(position);
        }
      }

      std::filesystem::remove(participantFile);
    }

    if (!states.empty()) {
      const auto& first = states.front().descriptor;
      throw std::runtime_error("Could not find file name information for " + std::to_string(states.size()) + " record(s), the first of which is for participant " + first.getParticipant().getLocalPseudonym().text()
          + ", column " + first.getColumn()
          + ", blinding timestamp " + std::to_string(first.getBlindingTimestamp().ticks_since_epoch<std::chrono::milliseconds>()));
    }
    std::filesystem::remove(legacyPristineFile);
    LOG(LOG_TAG, warning) << "Download directory metadata format upgraded. Please update your (offline) copies.";
  }

  if (!GetLegacyParticipantMetaFilePaths(mDownloadDirectory).empty()) {
    throw std::runtime_error("Participant metadata file(s) found in directory after conversion, or directory contains no pristine metadata file");
  }
}

DownloadMetadata::DownloadMetadata(const std::filesystem::path& downloadDirectory, std::shared_ptr<GlobalConfiguration> globalConfig, const Progress::OnCreation& onCreateProgress)
  : mGlobalConfig(globalConfig), mDownloadDirectory(std::filesystem::canonical(downloadDirectory)) {
  ensureFormatUpToDate();

  auto directory = getDirectory();
  if (std::filesystem::exists(directory)) {
    std::vector<std::filesystem::path> participantPaths;
    for (std::filesystem::directory_iterator i(directory); i != std::filesystem::directory_iterator(); ++i) {
      if (std::filesystem::is_directory(i->path()) && !std::filesystem::equivalent(i->path(), getDirectory())) {
        participantPaths.emplace_back(i->path());
      }
    }

    auto progress = Progress::Create(participantPaths.size(), onCreateProgress);
    for (const auto& participantPath : participantPaths) {
      auto participantDirectory = participantPath.filename();
      progress->advance("Participant " + participantDirectory.string());
      for (std::filesystem::directory_iterator j(participantPath); j != std::filesystem::directory_iterator(); ++j) {
        auto path = j->path();
        assert(path.string().ends_with(GetFilenameExtension()));
        auto filename = path.filename().string();
        assert(filename.starts_with(GetFilenamePrefix()));
        filename = filename.substr(0, filename.size() - GetFilenameExtension().size()); // Don't use path::stem, since it'll drop everything after the first period (.) instead of the last
        assert(std::filesystem::equivalent(path, directory / participantDirectory / (filename + GetFilenameExtension())));
        filename = filename.substr(GetFilenamePrefix().size());

        auto serialized = ReadFile(path);
        boost::property_tree::ptree properties;
        std::istringstream source(serialized);
        boost::property_tree::read_json(source, properties);
        auto record = DeserializeProperties<RecordState>(properties, MultiTypeTransform());

        // Cache deserialized value
        auto relative = (participantDirectory / filename).string();
        [[maybe_unused]] auto added = mSnapshotsByRelativePath->emplace(relative, Snapshot{ serialized, record }).second;
        assert(added);
        added = mRelativePathsByDescriptor->emplace(record.descriptor, relative).second;
        assert(added);
        assert(mSnapshotsByRelativePath->size() == mRelativePathsByDescriptor->size());
      }
    }
  }
}

std::filesystem::path DownloadMetadata::getDirectory() const {
  return mDownloadDirectory / DownloadMetadata::GetDirectoryName();
}

std::vector<RecordState> DownloadMetadata::getRecords() const {
  std::vector<RecordState> result;
  result.reserve(mSnapshotsByRelativePath->size());
  std::transform(mSnapshotsByRelativePath->cbegin(), mSnapshotsByRelativePath->cend(), std::back_inserter(result), [](const std::pair<const std::string, Snapshot>& entry) {return entry.second.record; });
  return result;
}

std::optional<XxHasher::Hash> DownloadMetadata::getHash(const RecordDescriptor& record) const {
  auto relative = getRelativePath(record);
  if (relative) {
    auto position = mSnapshotsByRelativePath->find(relative->string());
    if (position != mSnapshotsByRelativePath->cend()) {
      return position->second.record.hash;
    }
  }
  return std::nullopt;
}

std::optional<std::filesystem::path> DownloadMetadata::getRelativePath(const RecordDescriptor& record) const {
  auto position = mRelativePathsByDescriptor->find(record);
  if (position != mRelativePathsByDescriptor->cend()) {
    return position->second;
  }
  return std::nullopt;
}

void DownloadMetadata::add(const RecordDescriptor& record, const std::string& dataFileName, XxHasher::Hash hash) {
  auto participantDirectory = provideParticipantDirectory(record.getParticipant().getLocalPseudonym());
  auto path = participantDirectory / DataFileNameToMetaFileName(dataFileName);
  if (std::filesystem::exists(path)) {
    throw std::runtime_error("File already exists at " + path.string());
  }

  RecordState state{ record, hash };
  boost::property_tree::ptree properties;
  SerializeProperties(properties, state);
  std::ostringstream buffer;
  boost::property_tree::write_json(buffer, properties);
  auto serialized = buffer.str();

  WriteFile(path, serialized);

  auto relative = (participantDirectory.filename() / dataFileName).string();
  mSnapshotsByRelativePath->emplace(std::make_pair(relative, Snapshot{serialized, state}));
  mRelativePathsByDescriptor->emplace(state.descriptor, relative);
}

bool DownloadMetadata::remove(const RecordDescriptor& record) {
  auto position = mRelativePathsByDescriptor->find(record);
  if (position == mRelativePathsByDescriptor->cend()) {
    return false;
  }
  auto relative = std::filesystem::path(position->second);
  assert(relative.parent_path().string() == record.getParticipant().getLocalPseudonym().text()
    || relative.parent_path().string() == mGlobalConfig->getUserPseudonymFormat().makeUserPseudonym(record.getParticipant().getLocalPseudonym()));
  auto metaFileName = DataFileNameToMetaFileName(relative.filename().string());
  auto participantDirectory = getDirectory() / record.getParticipant().getLocalPseudonym().text();
  if(!std::filesystem::is_directory(participantDirectory)) {
    participantDirectory = getDirectory() / mGlobalConfig->getUserPseudonymFormat().makeUserPseudonym(record.getParticipant().getLocalPseudonym());
  }
  auto path =  participantDirectory / metaFileName;
  mSnapshotsByRelativePath->erase(position->second);
  mRelativePathsByDescriptor->erase(position);
  return std::filesystem::remove_all(path);
}
