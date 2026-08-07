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

const std::string LogTag = "Download metadata";

CheckedFileName DataFileNameToMetaFileName(const std::string& dataFileName) {
  return CheckedFileName(DownloadMetadata::GetFilenamePrefix() + dataFileName + DownloadMetadata::GetFilenameExtension());
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

const std::filesystem::path PristineStateFilename = DownloadMetadata::GetFilenamePrefix() + "pristine" + DownloadMetadata::GetFilenameExtension();
const std::filesystem::path LegacyParticipantMetaFilename = DownloadMetadata::GetFilenamePrefix() + "participant" + DownloadMetadata::GetFilenameExtension();

std::vector<std::filesystem::path> GetLegacyParticipantMetaFilePaths(const std::filesystem::path& downloadDirectory) {
  std::vector<std::filesystem::path> result;
  for (std::filesystem::directory_iterator i(downloadDirectory); i != std::filesystem::directory_iterator(); ++i) {
    if (i->is_directory()) {
      auto filename = i->path() / LegacyParticipantMetaFilename;
      if (std::filesystem::exists(filename)) {
        result.push_back(filename);
      }
    }
  }
  return result;
}

}

ParticipantIdentifier::ParticipantIdentifier(const PolymorphicPseudonym& polymorphic, const LocalPseudonym& local)
  : polymorphic_(polymorphic), local_(local) {
  if (polymorphic_ == PolymorphicPseudonym{}) {
    throw std::runtime_error("Participant identifier requires a nonempty polymorphic pseudonym");
  }
  if (local_ == LocalPseudonym{}) {
    throw std::runtime_error("Participant identifier requires a nonempty local pseudonym");
  }
}

RecordDescriptor::RecordDescriptor(const ParticipantIdentifier& participant, const std::string& column, const Timestamp& blindingTimestamp, const std::optional<Timestamp>& payloadBlindingTimestamp)
  : participant_(participant), column_(column), blindingTimestamp_(blindingTimestamp), payloadBlindingTimestamp_(payloadBlindingTimestamp) {
}

RecordDescriptor::RecordDescriptor(const ParticipantIdentifier& participant, const std::string& column, const Timestamp& blindingTimestamp, const std::map<std::string, MetadataXEntry>& extra, const std::optional<Timestamp>& payloadBlindingTimestamp)
  : participant_(participant), column_(column), blindingTimestamp_(blindingTimestamp), payloadBlindingTimestamp_(payloadBlindingTimestamp), extra_(extra) {
}

const Timestamp& RecordDescriptor::getPayloadBlindingTimestamp() const noexcept {
  if (payloadBlindingTimestamp_.has_value()) {
    return *payloadBlindingTimestamp_;
  }
  return blindingTimestamp_;
}

CheckedFileName RecordDescriptor::getFileName(bool includingExtension) const noexcept {
  auto name = this->getColumn();

  if (includingExtension) {
    auto found = this->getExtra().find("fileExtension");
    if (found != this->getExtra().end()) {
      name += found->second.plaintext();
    }
  }

  return CheckedFileName(name);
}

bool RecordDescriptor::operator==(const RecordDescriptor& other) const {
  return (participant_ == other.participant_) && (column_ == other.column_) && (blindingTimestamp_ == other.blindingTimestamp_);
}

CheckedPath DownloadMetadata::provideDirectory() const {
  auto result = getDirectory();
  if (std::filesystem::create_directories(result)) {
#ifdef _WIN32
    ::SetFileAttributesA(result.path().string().c_str(), FILE_ATTRIBUTE_HIDDEN);
#endif
  }
  return result;
}

CheckedPath DownloadMetadata::provideParticipantDirectory(const LocalPseudonym& localPseudonym) const {
  auto result = provideDirectory() / CheckedFileName(localPseudonym.text());
  if(std::filesystem::is_directory(result)) {
    return result;
  }

  auto pseudonym = globalConfig_->getUserPseudonymFormat().makeUserPseudonym(localPseudonym);
  result = provideDirectory() / CheckedFileName(pseudonym);
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

  auto legacyPristineFile = downloadDirectory_ / PristineStateFilename;
  if (std::filesystem::exists(legacyPristineFile)) {
    PEP_LOG(LogTag, Severity::Warning) << "Upgrading legacy download directory format.";
    boost::property_tree::ptree stateProperties;
    boost::property_tree::read_json(legacyPristineFile.string(), stateProperties);
    auto states = DeserializeProperties<std::vector<RecordState>>(stateProperties, "records", DeserializationContext());

    auto participantMetaFiles = GetLegacyParticipantMetaFilePaths(downloadDirectory_);
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
          auto timestamp = DeserializeProperties<Timestamp>(fileProperties.second, "timestamp", DeserializationContext());

          RecordDescriptor descriptor(id, column, timestamp);

          auto position = std::find_if(states.cbegin(), states.cend(), [&descriptor](const RecordState& candidate) {return candidate.descriptor == descriptor; });
          if (position == states.cend()) {
            throw std::runtime_error("Could not find pristine state for participant " + local.text()
                + ", column " + column
                + ", timestamp " + std::to_string(TicksSinceEpoch<std::chrono::milliseconds>(timestamp)));
          }

          if (position->hash) {
            add(descriptor, CheckedFileName(filename), *position->hash);
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
          + ", blinding timestamp " + std::to_string(TicksSinceEpoch<std::chrono::milliseconds>(first.getBlindingTimestamp())));
    }
    std::filesystem::remove(legacyPristineFile);
    PEP_LOG(LogTag, Severity::Warning) << "Download directory metadata format upgraded. Please update your (offline) copies.";
  }

  if (!GetLegacyParticipantMetaFilePaths(downloadDirectory_).empty()) {
    throw std::runtime_error("Participant metadata file(s) found in directory after conversion, or directory contains no pristine metadata file");
  }
}

DownloadMetadata::DownloadMetadata(const std::filesystem::path& downloadDirectory, std::shared_ptr<GlobalConfiguration> globalConfig, const Progress::OnCreation& onCreateProgress)
  : globalConfig_(globalConfig), downloadDirectory_(CheckedPath::FromTrusted(std::filesystem::canonical(downloadDirectory))) {
  ensureFormatUpToDate();

  auto directory = getDirectory();
  if (std::filesystem::exists(directory)) {
    std::vector<std::filesystem::path> participantPaths;
    for (std::filesystem::directory_iterator i(directory); i != std::filesystem::directory_iterator(); ++i) {
      if (i->is_directory() && !std::filesystem::equivalent(i->path(), getDirectory())) {
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
        filename = filename.substr(0, filename.size() - GetFilenameExtension().size());
        assert(std::filesystem::equivalent(path, directory / participantDirectory / (filename + GetFilenameExtension())));
        filename = filename.substr(GetFilenamePrefix().size());

        auto serialized = ReadFile(path);
        boost::property_tree::ptree properties;
        std::istringstream source(serialized);
        boost::property_tree::read_json(source, properties);
        auto record = DeserializeProperties<RecordState>(properties, DeserializationContext());

        // Cache deserialized value
        auto relative = CheckedPath::FromTrusted(participantDirectory) / CheckedFileName(filename);
        [[maybe_unused]] auto added = snapshotsByRelativePath_->emplace(relative, Snapshot{ serialized, record }).second;
        assert(added);
        added = relativePathsByDescriptor_->emplace(record.descriptor, relative).second;
        assert(added);
        assert(snapshotsByRelativePath_->size() == relativePathsByDescriptor_->size());
      }
    }
  }
}

CheckedPath DownloadMetadata::getDirectory() const {
  return downloadDirectory_ / CheckedFileName(DownloadMetadata::GetDirectoryName());
}

std::vector<RecordState> DownloadMetadata::getRecords() const {
  using namespace std::ranges;
  return RangeToVector(*snapshotsByRelativePath_
    | views::values
    | views::transform(std::mem_fn(&Snapshot::record)));
}

std::optional<XxHasher::Hash> DownloadMetadata::getHash(const RecordDescriptor& record) const {
  auto relative = getRelativePath(record);
  if (relative) {
    auto position = snapshotsByRelativePath_->find(*relative);
    if (position != snapshotsByRelativePath_->cend()) {
      return position->second.record.hash;
    }
  }
  return std::nullopt;
}

std::optional<CheckedRelativeFilePath> DownloadMetadata::getRelativePath(const RecordDescriptor& record) const {
  auto position = relativePathsByDescriptor_->find(record);
  if (position != relativePathsByDescriptor_->cend()) {
    return position->second;
  }
  return std::nullopt;
}

void DownloadMetadata::add(const RecordDescriptor& record, const CheckedFileName& dataFileName, XxHasher::Hash hash) {
  auto participantDirectory = provideParticipantDirectory(record.getParticipant().getLocalPseudonym());
  auto path = participantDirectory / DataFileNameToMetaFileName(dataFileName.path().string());
  if (std::filesystem::exists(path)) {
    throw std::runtime_error("File already exists at " + path.text());
  }

  RecordState state{ record, hash };
  boost::property_tree::ptree properties;
  SerializeProperties(properties, state);
  std::ostringstream buffer;
  boost::property_tree::write_json(buffer, properties);
  auto serialized = std::move(buffer).str();

  WriteFile(path, serialized);

  auto relative = participantDirectory.fileName() / dataFileName;
  snapshotsByRelativePath_->emplace(relative, Snapshot{serialized, state});
  relativePathsByDescriptor_->emplace(state.descriptor, relative);
}

bool DownloadMetadata::remove(const RecordDescriptor& record) {
  auto position = relativePathsByDescriptor_->find(record);
  if (position == relativePathsByDescriptor_->cend()) {
    return false;
  }
  const std::filesystem::path& relative = position->second;
  assert(relative.parent_path().string() == record.getParticipant().getLocalPseudonym().text()
    || relative.parent_path().string() == globalConfig_->getUserPseudonymFormat().makeUserPseudonym(record.getParticipant().getLocalPseudonym()));
  auto metaFileName = DataFileNameToMetaFileName(relative.filename().string());
  auto participantDirectory = getDirectory() / CheckedFileName(record.getParticipant().getLocalPseudonym().text());
  if(!std::filesystem::is_directory(participantDirectory)) {
    participantDirectory = getDirectory() / CheckedFileName(globalConfig_->getUserPseudonymFormat().makeUserPseudonym(record.getParticipant().getLocalPseudonym()));
  }
  CheckedPath path = participantDirectory / metaFileName;
  snapshotsByRelativePath_->erase(position->second);
  relativePathsByDescriptor_->erase(position);
  return std::filesystem::remove_all(path) != 0;
}
