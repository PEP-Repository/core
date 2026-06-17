#include <pep/cli/DownloadProcessor.hpp>

#include <pep/utils/Defer.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/File.hpp>
#include <pep/utils/MiscUtil.hpp>
#include <pep/archiving/Pseudonymiser.hpp>
#include <pep/archiving/DirectoryArchive.hpp>
#include <pep/archiving/HashedArchive.hpp>
#include <pep/storagefacility/Constants.hpp>
#include <pep/utils/PropertySerializer.hpp>
#include <pep/rsk-pep/Pseudonyms.PropertySerializers.hpp>
#include <pep/core-client/CoreClient.hpp>

#include <fstream>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <rxcpp/operators/rx-reduce.hpp>
#include <pep/archiving/Tar.hpp>
#include <utility>

using namespace pep;
using namespace pep::cli;

namespace {

const std::string LogTag = "Download Data";
const std::string SPECIFICATION_FILENAME = DownloadMetadata::GetFilenamePrefix() + "specification" + DownloadMetadata::GetFilenameExtension();

std::filesystem::path ValidateDirectory(const std::filesystem::path& raw) {
  auto result = std::filesystem::absolute(raw);
  if (std::filesystem::exists(result)) {
    if (!std::filesystem::is_directory(result)) {
      throw std::runtime_error("Cannot use a non-directory " + raw.string() + " as a download directory");
    }
  }
  else {
    std::filesystem::create_directories(result);
  }
  return result;
}

}


DownloadDirectory::DownloadDirectory(const std::filesystem::path& root, std::shared_ptr<GlobalConfiguration> globalConfig, const Progress::OnCreation& onCreateProgress)
  : root_(ValidateDirectory(root)), metadata_(root_, globalConfig, onCreateProgress), globalConfig_(globalConfig) {
  auto spec = tryReadSpecification();
  if (!spec) {
    throw std::runtime_error("Directory " + root_.string() + " is not a PEP download directory");
  }
  applyFileExtensions_ = spec->applyFileExtensions;
}

DownloadDirectory::DownloadDirectory(const std::filesystem::path& root, std::shared_ptr<CoreClient> client, const ContentSpecification& content,
    std::shared_ptr<GlobalConfiguration> globalConfig, bool applyFileExtensions)
  : root_(ValidateDirectory(root)), applyFileExtensions_(applyFileExtensions), metadata_(root, globalConfig), globalConfig_(globalConfig) {
  if (std::filesystem::directory_iterator(root_) != std::filesystem::directory_iterator()) {
    throw std::runtime_error("Cannot initialize a new download in nonempty directory " + root_.string());
  }
  Specification spec;
  spec.content = content;
  spec.accessGroup = client->getEnrolledGroup();
  spec.applyFileExtensions = applyFileExtensions_;
  WriteFile(getSpecificationFilePath(), spec.toString());
}

std::filesystem::path DownloadDirectory::getSpecificationFilePath() const {
  return root_ / SPECIFICATION_FILENAME;
}

std::optional<DownloadDirectory::Specification> DownloadDirectory::tryReadSpecification() const {
  auto content = ReadFileIfExists(getSpecificationFilePath());
  if (content) {
    return DownloadDirectory::Specification::FromString(*content);
  }
  return std::nullopt;
}

std::filesystem::path DownloadDirectory::getParticipantDirectory(const ParticipantIdentifier& id) const {
  auto result = root_ / id.getLocalPseudonym().text();
  if (std::filesystem::is_directory(result)) {
    return result;
  }
  return root_ / globalConfig_->getUserPseudonymFormat().makeUserPseudonym(id.getLocalPseudonym());
}

std::optional<std::filesystem::path> DownloadDirectory::getParticipantDirectoryIfExists(const ParticipantIdentifier& id) const {
  auto result = getParticipantDirectory(id);
  if (std::filesystem::is_directory(result)) {
    return result;
  }
  return std::nullopt;
}

std::filesystem::path DownloadDirectory::provideParticipantDirectory(const ParticipantIdentifier& id) {
  auto result = getParticipantDirectory(id);
  std::filesystem::create_directories(result);
  return result;
}

DownloadDirectory::Specification DownloadDirectory::getSpecification() const {
  auto result = tryReadSpecification();
  if (!result) {
    throw std::runtime_error("Specification file could not be read from " + getSpecificationFilePath().string());
  }
  return *result;
}

std::optional<std::filesystem::path> DownloadDirectory::getRecordFileName(const RecordDescriptor& descriptor, bool absolute) const {
  auto relativePath = metadata_.getRelativePath(descriptor);
  if (!relativePath.has_value() || !absolute) {
    return relativePath;
  }
  return root_ / *relativePath;
}

void DownloadDirectory::clear() {
  std::vector<std::filesystem::path> contents{ std::filesystem::directory_iterator(root_), std::filesystem::directory_iterator() };
  for (const auto& entry : contents) {
    if (entry != getSpecificationFilePath()) {
      std::filesystem::remove_all(entry);
    }
  }
}

bool DownloadDirectory::deleteRecord(const RecordDescriptor& descriptor) {
  auto result = false;
  auto dataPath = getRecordFileName(descriptor);

  if (dataPath) {
    assert(std::filesystem::exists(*dataPath));
    std::filesystem::remove_all(*dataPath);
    result = true;
  }

  return result;
}

bool DownloadDirectory::renameRecord(const RecordDescriptor& descriptor, const std::filesystem::path& newPath) {
  auto result = false;
  auto dataPath = getRecordFileName(descriptor);

  if (dataPath) {
    assert(std::filesystem::exists(*dataPath));
    if (*dataPath != newPath) {
      std::filesystem::rename(*dataPath, newPath);
    }
    result = true;
  }

  return result;
}

template <>
class pep::PropertySerializer<DownloadDirectory::ContentSpecification> : public PropertySerializerByReference<DownloadDirectory::ContentSpecification> {
public:
  void read(DownloadDirectory::ContentSpecification& destination, const boost::property_tree::ptree& source, const DeserializationContext& context) const override {
    DeserializeProperties(destination.columnGroups, source, "column-groups", context);
    DeserializeProperties(destination.columns, source, "columns", context);
    DeserializeProperties(destination.groups, source, "participant-groups", context);
    DeserializeProperties(destination.pps, source, "participants", context);
  }

  void write(boost::property_tree::ptree& destination, const DownloadDirectory::ContentSpecification& value) const override {
    SerializeProperties(destination, "column-groups", value.columnGroups);
    SerializeProperties(destination, "columns", value.columns);
    SerializeProperties(destination, "participant-groups", value.groups);
    SerializeProperties(destination, "participants", value.pps);
  }
};

template <>
class pep::PropertySerializer<DownloadDirectory::Specification> : public PropertySerializerByReference<DownloadDirectory::Specification> {
public:
  void read(DownloadDirectory::Specification& destination, const boost::property_tree::ptree& source, const DeserializationContext& context) const override {
    DeserializeProperties(destination.accessGroup, source, "access-group", context);
    DeserializeProperties(destination.content, source, "content", context);

    // Backward compatibility: the download directory may have been created by a PEP version that didn't support file extensions yet.
    // We may therefore be reading a property_tree that does not contain an "apply-file-extensions" node.
    // In this case, keep the directory in the same format by _not_ applying file extensions.
    destination.applyFileExtensions = DeserializeProperties<std::optional<bool>>(source, "apply-file-extensions", context).value_or(false);
  }

  void write(boost::property_tree::ptree& destination, const DownloadDirectory::Specification& value) const override {
    SerializeProperties(destination, "access-group", value.accessGroup);
    SerializeProperties(destination, "content", value.content);
    SerializeProperties(destination, "apply-file-extensions", value.applyFileExtensions);
  }
};

std::vector<DownloadDirectory::NonPristineEntry> DownloadDirectory::getNonPristineEntries(const Progress::OnCreation& onCreateProgress) const {
  std::vector<DownloadDirectory::NonPristineEntry> result;

  // Check entries that should be there
  auto pristine = metadata_.getRecords();
  filesystem::SetOfExistingPaths dirs, files;
  auto progress = Progress::Create(pristine.size(), onCreateProgress);
  for (const auto& entry : pristine) {
    this->trackExistingPaths(dirs, files, entry.descriptor);
    auto current = getCurrentDataHash(entry.descriptor);
    auto filename = getRecordFileName(entry.descriptor);
    progress->advance(1U, GetOptionalValue(this->getRecordFileName(entry.descriptor, false), [](const std::filesystem::path& path) {return path.string(); }));
    if (entry.hash != current) {
      result.emplace_back(entry.descriptor, filename);
    }
  }

  // Check entries that shouldn't be there
  // TODO: report Progress for this?
  auto unknown = this->getUnknownContents(dirs, files);
  result.reserve(result.size() + unknown.size());
  std::transform(unknown.begin(), unknown.end(), std::back_inserter(result), [](const std::filesystem::path& path) {
    return NonPristineEntry{ std::nullopt, path };
    });

  progress->advanceToCompletion();
  return result;
}

void DownloadDirectory::trackExistingPaths(filesystem::SetOfExistingPaths& dirs, filesystem::SetOfExistingPaths& files, const RecordDescriptor& descriptor) const {
  auto addToSet = [](filesystem::SetOfExistingPaths& paths, const std::filesystem::path& existing) {
    assert(exists(existing));
    return paths.insert(existing).second;
    };

  addToSet(dirs, this->getParticipantDirectory(descriptor.getParticipant()));
  if (auto file = this->getRecordFileName(descriptor)) {
    [[maybe_unused]] auto added = addToSet(files, *file);
    assert(added);
  }
}

filesystem::SetOfExistingPaths DownloadDirectory::getUnknownContents() const {
  // List contents that should be there
  filesystem::SetOfExistingPaths dirs, files;
  for (const auto& record : metadata_.getRecords()) {
    this->trackExistingPaths(dirs, files, record.descriptor);
  }
  // Return contents that shouldn't be there
  return this->getUnknownContents(dirs, files);
}

filesystem::SetOfExistingPaths DownloadDirectory::getUnknownContents(const filesystem::SetOfExistingPaths& knownDirs, const filesystem::SetOfExistingPaths& knownFiles) const {
  // Iterate over actual contents, collecting entries that shouldn't be there
  filesystem::SetOfExistingPaths result;
  for (std::filesystem::directory_iterator i(root_); i != std::filesystem::directory_iterator(); ++i) {
    if (is_directory(i->path())) {
      if (!equivalent(i->path(), metadata_.getDirectory())) {
        if (!knownDirs.contains(i->path())) {
          result.insert(i->path());
        } else {
          for (std::filesystem::directory_iterator j(i->path()); j != std::filesystem::directory_iterator(); ++j) {
            if (!knownFiles.contains(j->path())) {
              result.insert(j->path());
            }
          }
        }
      }
    } else if (!equivalent(i->path(), this->getSpecificationFilePath())) {
      result.insert(i->path());
    }
  }

  return result;
}

std::vector<RecordDescriptor> DownloadDirectory::getRecords(const std::function<bool(const RecordDescriptor&)>& match) const {
  std::vector<RecordDescriptor> result;

  auto pristine = metadata_.getRecords(); // TODO: don't rely on pristine data here
  std::transform(pristine.cbegin(), pristine.cend(), std::back_inserter(result), [](const RecordState& state) {return state.descriptor; });
  auto removed = std::remove_if(result.begin(), result.end(), [&match](const RecordDescriptor& candidate) {return !match(candidate); });
  result.erase(removed, result.cend());

  return result;
}

std::vector<RecordDescriptor> DownloadDirectory::list() const {
  return getRecords([](const RecordDescriptor&) {return true; });
}

std::vector<RecordDescriptor> DownloadDirectory::list(const ParticipantIdentifier& id) const {
  return getRecords([&id](const RecordDescriptor& candidate) {return candidate.getParticipant() == id; });
}

std::vector<RecordDescriptor> DownloadDirectory::list(const ParticipantIdentifier& id, const std::string& column) const {
  return getRecords([&id, &column](const RecordDescriptor& candidate) {return candidate.getParticipant() == id && candidate.getColumn() == column; });
}

std::optional<XxHasher::Hash> DownloadDirectory::getCurrentDataHash(const std::filesystem::path& path) const {
  if (std::filesystem::exists(path)) {
    if(std::filesystem::is_directory(path)) {
      return HashedArchive::HashDirectory(path);
    }
    else {
      std::ifstream stream(path.string().c_str(), std::ios::in | std::ios::binary);
      return XxHasher(HashedArchive::DOWNLOAD_HASH_SEED)
        .update(stream)
        .digest();
    }
  }
  return std::nullopt;
}

std::optional<XxHasher::Hash> DownloadDirectory::getCurrentDataHash(const RecordDescriptor& descriptor) const {
  auto filename = getRecordFileName(descriptor);
  if (filename) {
    return getCurrentDataHash(*filename);
  }
  return std::nullopt;
}

void DownloadDirectory::setStoredDataHash(const RecordDescriptor& record, const std::filesystem::path& path, const std::string& fileName, XxHasher::Hash hash) {
  auto actual = getCurrentDataHash(path);
  if (!actual) {
    throw std::runtime_error("Data not stored");
  }
  if (hash != *actual) {
    throw std::runtime_error("Data corrupted during storage");
  }
  metadata_.add(record, fileName, hash);
}

bool DownloadDirectory::hasPristineData(const RecordDescriptor& descriptor) const {
  auto current = getCurrentDataHash(descriptor);
  if (!current) {
    return false;
  }
  return current == metadata_.getHash(descriptor);
}

std::filesystem::path DownloadDirectory::getDataStoragePath(const RecordDescriptor& descriptor) {
  auto path = provideParticipantDirectory(descriptor.getParticipant());
  path.append(descriptor.getFileName(applyFileExtensions_));

  if (std::filesystem::exists(path)) {
    throw std::runtime_error("Data storage path already exists at " + path.string());
  }

  return path;
}

std::shared_ptr<DownloadDirectory::RecordStorageStream> DownloadDirectory::create(RecordDescriptor descriptor, bool pseudonymisationRequired, bool archiveExtractionRequired, std::uint64_t fileSize) {
  remove(descriptor);

  assert(!getRecordFileName(descriptor));

  auto path = this->getDataStoragePath(descriptor);
  return std::shared_ptr<RecordStorageStream>(new RecordStorageStream(shared_from_this(), std::move(descriptor), path, pseudonymisationRequired, archiveExtractionRequired, fileSize));
}

bool DownloadDirectory::remove(const RecordDescriptor& descriptor) {
  auto result = deleteRecord(descriptor);
  metadata_.remove(descriptor);
  return result;
}

bool DownloadDirectory::update(const RecordDescriptor& descriptor, const RecordDescriptor& updated) {
  auto hash = metadata_.getHash(descriptor);
  if (hash == std::nullopt) {
    throw std::runtime_error("Cannot find record descriptor to update");
  }
  auto newPath = this->getDataStoragePath(updated);
  auto result = renameRecord(descriptor, newPath);
  metadata_.remove(descriptor);
  metadata_.add(updated, newPath.filename().string(), *hash);
  return result;
}

rxcpp::observable<FakeVoid> DownloadDirectory::pull(std::shared_ptr<CoreClient> source, const PullOptions& options, const Progress::OnCreation& onCreateProgress) {
  auto previous = getSpecification().accessGroup, current = source->getEnrolledGroup();
  if (previous != current) {
    throw std::runtime_error("Cannot pull download for access group " + previous + " when enrolled for access group " + current);
  }

  return DownloadProcessor::Create(shared_from_this(), globalConfig_)->update(source, options, onCreateProgress);
}

DownloadDirectory::Specification DownloadDirectory::Specification::FromString(const std::string& value) {
  DownloadDirectory::Specification result;

  std::istringstream json(value);
  boost::property_tree::ptree root;
  boost::property_tree::read_json(json, root);

  return DeserializeProperties<DownloadDirectory::Specification>(root, DeserializationContext());
}

std::string DownloadDirectory::Specification::toString() const {
  boost::property_tree::ptree root;
  SerializeProperties(root, *this);

  std::ostringstream result;
  boost::property_tree::write_json(result, root);
  return std::move(result).str();
}


DownloadDirectory::RecordStorageStream::RecordStorageStream(std::shared_ptr<DownloadDirectory> destination, RecordDescriptor descriptor, std::filesystem::path path, bool pseudonymisationRequired, bool archiveExtractionRequired, std::uint64_t fileSize)
  : destination_(std::move(destination)), descriptor_(std::move(descriptor)), path_(std::move(path)), fileName_(path_.filename().string()), fileSize_(fileSize), hasher_(HashedArchive::DOWNLOAD_HASH_SEED), pseudonymisationRequired_(pseudonymisationRequired), archiveExtractionRequired_(archiveExtractionRequired) {

  raw_ = std::make_shared<std::ofstream>(path_.string(), std::ios::out | std::ios::binary);
  if (!raw_->is_open()) {
    throw std::system_error(errno, std::generic_category(), "Failed to open " + path_.string());
  }
}

DownloadDirectory::RecordStorageStream::~RecordStorageStream() noexcept {
  if (raw_) {
    PEP_LOG(LogTag, Severity::Error) << "Error destructing RecordStorageStream: uncommitted record at \"" + path_.string() << '"'; // TODO: improve
  }
}

std::filesystem::path DownloadDirectory::RecordStorageStream::getRelativePath() const {
  return std::filesystem::relative(path_, destination_->getPath());
}

void DownloadDirectory::RecordStorageStream::write(const std::string& part, std::shared_ptr<GlobalConfiguration> globalConfig) {
  if (!raw_) {
    throw std::runtime_error("Cannot write to record stored at " + path_.string() + " after it has been committed");
  }
  if(!pseudonymisationRequired_) { //Postpone hashing until after/during the depseudonymisation, since we want to hash the depseudonymised data
    hasher_.update(part.data(), part.size());
  }

  (*raw_) << part;
  written_ += part.size();

  // Assert that this code is only reached as long as've written less or equal to signaled filesize
  assert(written_ <= fileSize_);

  if (written_ >= fileSize_) {
    this->commit(globalConfig);
  }
}

void DownloadDirectory::RecordStorageStream::commit(std::shared_ptr<GlobalConfiguration> globalConfig) {
  if (!raw_) {
    throw std::runtime_error("Record has already been committed and stored at " + path_.string());
  }
  raw_ = nullptr;
  XxHasher::Hash hash{};
  std::filesystem::path path;
  std::optional<Pseudonymiser> pseudonymiser{std::nullopt};

  if (archiveExtractionRequired_) {
    // path_ and the outpath defined below might collide. Add the .raw extension to ensure uniqueness.
    auto raw = std::filesystem::path(path_.string() + ".raw");
    std::filesystem::rename(path_, raw);
    path_ = raw;

    auto outpath = path_.parent_path() / descriptor_.getColumn();
    auto directoryArchive = DirectoryArchive::Create(outpath);
    auto hashedArchive = HashedArchive::Create(directoryArchive);

    auto in = std::ifstream(path_.string(), std::ios::binary);

    PEP_DEFER( // code is executed when this scope ends, with or without exceptions.
      in.close();
      std::filesystem::remove(path_); // The raw .raw file
    );

    auto temppath = std::filesystem::path(outpath.string() + ".tmp");
    PEP_DEFER(
      std::filesystem::remove_all(temppath); // the extracted directory without the new pseudonyms.
    );

    Tar::Extract(in, temppath);

    if (pseudonymisationRequired_) {
      auto localPseudonym = globalConfig->getUserPseudonymFormat().makeUserPseudonym(descriptor_.getParticipant().getLocalPseudonym());
      pseudonymiser = Pseudonymiser(descriptor_.getExtra().at("pseudonymPlaceholder").plaintext(), localPseudonym);
    }

    WriteToArchive(temppath, hashedArchive, pseudonymiser);

    path = outpath;
    hash = hashedArchive->digest();
  }
  else {
    //Single File
    if (pseudonymisationRequired_) {
      auto localPseudonym = globalConfig->getUserPseudonymFormat().makeUserPseudonym(descriptor_.getParticipant().getLocalPseudonym());
      pseudonymiser = Pseudonymiser(descriptor_.getExtra().at("pseudonymPlaceholder").plaintext(), localPseudonym);

      auto in = std::ifstream(path_.string(), std::ios::binary);
      auto temppath = std::filesystem::path(path_.string() + ".tmp");
      auto tempOut = std::ofstream(temppath.string(), std::ios::binary);

      // Hashing has been postponed, so do it now.
      auto writeAndHash = [&tempOut, &hasher = hasher_](const char* c, const std::streamsize l) {tempOut.write(c, l); hasher.update(c, static_cast<size_t>(l)); tempOut.flush();};
      pseudonymiser->pseudonymise(in, writeAndHash);

      in.close();
      tempOut.close();
      std::filesystem::remove(path_);
      std::filesystem::rename(temppath, path_);
    }

    hash = hasher_.digest();
    path = path_;
  }

  try {
    destination_->setStoredDataHash(descriptor_, path, fileName_, hash);
  }
  catch (...) {
    PEP_LOG(LogTag, Severity::Error) << "Could not write stored data hash for record at \"" << path_.string() << "\": " << GetExceptionMessage(std::current_exception());
    throw;
  }
}

bool DownloadDirectory::RecordStorageStream::isCommitted() const noexcept
{
  return !raw_;
}
