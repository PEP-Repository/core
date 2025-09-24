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

const std::string LOG_TAG = "Download Data";
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


DownloadDirectory::DownloadDirectory(const std::filesystem::path& root, std::shared_ptr<GlobalConfiguration> globalConfig)
  : mRoot(ValidateDirectory(root)), mMetadata(mRoot, globalConfig), mGlobalConfig(globalConfig) {
  auto spec = tryReadSpecification();
  if (!spec) {
    throw std::runtime_error("Directory " + mRoot.string() + " is not a PEP download directory");
  }
  mApplyFileExtensions = spec->applyFileExtensions;
}

DownloadDirectory::DownloadDirectory(const std::filesystem::path& root, std::shared_ptr<CoreClient> client, const ContentSpecification& content,
    std::shared_ptr<GlobalConfiguration> globalConfig, bool applyFileExtensions)
  : mRoot(ValidateDirectory(root)), mApplyFileExtensions(applyFileExtensions), mMetadata(root, globalConfig), mGlobalConfig(globalConfig) {
  if (std::filesystem::directory_iterator(mRoot) != std::filesystem::directory_iterator()) {
    throw std::runtime_error("Cannot initialize a new download in nonempty directory " + mRoot.string());
  }
  Specification spec;
  spec.content = content;
  spec.accessGroup = client->getEnrolledGroup();
  spec.applyFileExtensions = mApplyFileExtensions;
  WriteFile(getSpecificationFilePath(), spec.toString());
}

std::filesystem::path DownloadDirectory::getSpecificationFilePath() const {
  return mRoot / SPECIFICATION_FILENAME;
}

std::optional<DownloadDirectory::Specification> DownloadDirectory::tryReadSpecification() const {
  auto content = ReadFileIfExists(getSpecificationFilePath());
  if (content) {
    return DownloadDirectory::Specification::FromString(*content);
  }
  return std::nullopt;
}

std::filesystem::path DownloadDirectory::getParticipantDirectory(const ParticipantIdentifier& id) const {
  auto result = mRoot / id.getLocalPseudonym().text();
  if (std::filesystem::is_directory(result)) {
    return result;
  }
  return mRoot / mGlobalConfig->getUserPseudonymFormat().makeUserPseudonym(id.getLocalPseudonym());
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

std::optional<std::filesystem::path> DownloadDirectory::getRecordFileName(const RecordDescriptor& descriptor) const {
  auto relative = mMetadata.getRelativePath(descriptor);
  if (relative) {
    return mRoot / *relative;
  }
  return std::nullopt;
}

void DownloadDirectory::clear() {
  std::vector<std::filesystem::path> contents{ std::filesystem::directory_iterator(mRoot), std::filesystem::directory_iterator() };
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
  void read(DownloadDirectory::ContentSpecification& destination, const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const override {
    DeserializeProperties(destination.columnGroups, source, "column-groups", transform);
    DeserializeProperties(destination.columns, source, "columns", transform);
    DeserializeProperties(destination.groups, source, "participant-groups", transform);
    DeserializeProperties(destination.pps, source, "participants", transform);
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
  void read(DownloadDirectory::Specification& destination, const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const override {
    DeserializeProperties(destination.accessGroup, source, "access-group", transform);
    DeserializeProperties(destination.content, source, "content", transform);

    // Backward compatibility: the download directory may have been created by a PEP version that didn't support file extensions yet.
    // We may therefore be reading a property_tree that does not contain an "apply-file-extensions" node.
    // In this case, keep the directory in the same format by _not_ applying file extensions.
    destination.applyFileExtensions = DeserializeProperties<std::optional<bool>>(source, "apply-file-extensions", transform).value_or(false);
  }

  void write(boost::property_tree::ptree& destination, const DownloadDirectory::Specification& value) const override {
    SerializeProperties(destination, "access-group", value.accessGroup);
    SerializeProperties(destination, "content", value.content);
    SerializeProperties(destination, "apply-file-extensions", value.applyFileExtensions);
  }
};

std::optional<std::string> DownloadDirectory::describeFirstNonPristineEntry(const Progress::OnCreation& onCreateProgress) const {
  // TODO: check if directory contains files/directories other than pristine data
  auto pristine = mMetadata.getRecords();
  auto progress = Progress::Create(pristine.size(), onCreateProgress);

  for (const auto& entry : pristine) {
    auto current = getCurrentDataHash(entry.descriptor);
    auto filename = getRecordFileName(entry.descriptor);
    progress->advance(1U, GetOptionalValue(filename, [](const std::filesystem::path& path) {return path.string(); }));
    if (entry.hash != current) {
      if (filename == std::nullopt) {
        return "absent file for participant " + entry.descriptor.getParticipant().getLocalPseudonym().text()
          + ", column " + entry.descriptor.getColumn();
      }
      return "file " + filename->string();
    }
  }

  progress->advanceToCompletion();
  return std::nullopt;
}

std::vector<RecordDescriptor> DownloadDirectory::getRecords(const std::function<bool(const RecordDescriptor&)>& match) const {
  std::vector<RecordDescriptor> result;

  auto pristine = mMetadata.getRecords(); // TODO: don't rely on pristine data here
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
  mMetadata.add(record, fileName, hash);
}

bool DownloadDirectory::hasPristineData(const RecordDescriptor& descriptor) const {
  auto current = getCurrentDataHash(descriptor);
  if (!current) {
    return false;
  }
  return current == mMetadata.getHash(descriptor);
}

std::filesystem::path DownloadDirectory::getDataStoragePath(const RecordDescriptor& descriptor) {
  auto path = provideParticipantDirectory(descriptor.getParticipant());
  path.append(descriptor.getFileName(mApplyFileExtensions));

  if (std::filesystem::exists(path)) {
    throw std::runtime_error("Data storage path already exists at " + path.string());
  }

  return path;
}

std::shared_ptr<DownloadDirectory::RecordStorageStream> DownloadDirectory::create(RecordDescriptor descriptor, bool pseudonymisationRequired, bool archiveExtractionRequired, size_t fileSize) {
  remove(descriptor);

  assert(!getRecordFileName(descriptor));

  auto path = this->getDataStoragePath(descriptor);
  return std::shared_ptr<RecordStorageStream>(new RecordStorageStream(shared_from_this(), std::move(descriptor), path, pseudonymisationRequired, archiveExtractionRequired, fileSize));
}

bool DownloadDirectory::remove(const RecordDescriptor& descriptor) {
  auto result = deleteRecord(descriptor);
  mMetadata.remove(descriptor);
  return result;
}

bool DownloadDirectory::update(const RecordDescriptor& descriptor, const RecordDescriptor& updated) {
  auto hash = mMetadata.getHash(descriptor);
  if (hash == std::nullopt) {
    throw std::runtime_error("Cannot find record descriptor to update");
  }
  auto newPath = this->getDataStoragePath(updated);
  auto result = renameRecord(descriptor, newPath);
  mMetadata.remove(descriptor);
  mMetadata.add(updated, newPath.filename().string(), *hash);
  return result;
}

rxcpp::observable<FakeVoid> DownloadDirectory::pull(std::shared_ptr<CoreClient> source, const PullOptions& options, const Progress::OnCreation& onCreateProgress) {
  auto previous = getSpecification().accessGroup, current = source->getEnrolledGroup();
  if (previous != current) {
    throw std::runtime_error("Cannot pull download for access group " + previous + " when enrolled for access group " + current);
  }

  return DownloadProcessor::Create(shared_from_this(), mGlobalConfig)->update(source, options, onCreateProgress);
}

DownloadDirectory::Specification DownloadDirectory::Specification::FromString(const std::string& value) {
  DownloadDirectory::Specification result;

  std::istringstream json(value);
  boost::property_tree::ptree root;
  boost::property_tree::read_json(json, root);

  return DeserializeProperties<DownloadDirectory::Specification>(root, MultiTypeTransform());
}

std::string DownloadDirectory::Specification::toString() const {
  boost::property_tree::ptree root;
  SerializeProperties(root, *this);

  std::ostringstream result;
  boost::property_tree::write_json(result, root);
  return result.str();
}


DownloadDirectory::RecordStorageStream::RecordStorageStream(std::shared_ptr<DownloadDirectory> destination, RecordDescriptor descriptor, std::filesystem::path path, bool pseudonymisationRequired, bool archiveExtractionRequired, size_t fileSize)
  : mDestination(std::move(destination)), mDescriptor(std::move(descriptor)), mPath(std::move(path)), mFileName(mPath.filename().string()), mFileSize(fileSize), mHasher(HashedArchive::DOWNLOAD_HASH_SEED), mPseudonymisationRequired(pseudonymisationRequired), mArchiveExtractingRequired(archiveExtractionRequired) {
  
  mRaw = std::make_shared<std::ofstream>(mPath.string(), std::ios::out | std::ios::binary);
  if (!mRaw->is_open()) {
    throw std::system_error(errno, std::generic_category(), "Failed to open " + mPath.string());
  }
}

DownloadDirectory::RecordStorageStream::~RecordStorageStream() noexcept {
  if (mRaw) {
    LOG(LOG_TAG, error) << "Error destructing RecordStorageStream: uncommitted record at " + mPath.string(); // TODO: improve
  }
}

std::filesystem::path DownloadDirectory::RecordStorageStream::getRelativePath() const {
  return std::filesystem::relative(mPath, mDestination->getPath());
}

void DownloadDirectory::RecordStorageStream::write(const std::string& part, std::shared_ptr<GlobalConfiguration> globalConfig) {
  if (!mRaw) {
    throw std::runtime_error("Cannot write to record stored at " + mPath.string() + " after it has been committed");
  }
  if(!mPseudonymisationRequired) { //Postpone hashing until after/during the depseudonymisation, since we want to hash the depseudonymised data
    mHasher.update(part.data(), part.size());
  }

  (*mRaw) << part;
  mWritten += part.size();

  // Assert that this code is only reached as long as've written less or equal to signaled filesize
  assert(mWritten <= mFileSize);

  if (mWritten >= mFileSize) {
    this->commit(globalConfig);
  }
}

void DownloadDirectory::RecordStorageStream::commit(std::shared_ptr<GlobalConfiguration> globalConfig) {
  if (!mRaw) {
    throw std::runtime_error("Record has already been committed and stored at " + mPath.string());
  }
  mRaw = nullptr;
  XxHasher::Hash hash;
  std::filesystem::path path;
  std::optional<Pseudonymiser> pseudonymiser{std::nullopt};

  if (mArchiveExtractingRequired) {
    // mPath and the outpath defined below might collide. Add the .raw extension to ensure uniqueness.
    auto raw = std::filesystem::path(mPath.string() + ".raw");
    std::filesystem::rename(mPath, raw);
    mPath = raw;

    auto outpath = mPath.parent_path() / mDescriptor.getColumn();
    auto directoryArchive = DirectoryArchive::Create(outpath);
    auto hashedArchive = HashedArchive::Create(directoryArchive);

    auto in = std::ifstream(mPath.string(), std::ios::binary);

    PEP_DEFER( // code is executed when this scope ends, with or without exceptions.
      in.close();
      std::filesystem::remove(mPath); // The raw .raw file
    );

    auto temppath = std::filesystem::path(outpath.string() + ".tmp");
    PEP_DEFER(
      std::filesystem::remove_all(temppath); // the extracted directory without the new pseudonyms.
    );

    Tar::Extract(in, temppath);

    if (mPseudonymisationRequired) {
      auto localPseudonym = globalConfig->getUserPseudonymFormat().makeUserPseudonym(mDescriptor.getParticipant().getLocalPseudonym());
      pseudonymiser = Pseudonymiser(mDescriptor.getExtra().at("pseudonymPlaceholder").plaintext(), localPseudonym);
    }

    WriteToArchive(temppath, hashedArchive, pseudonymiser);

    path = outpath;
    hash = hashedArchive->digest();
  }
  else {
    //Single File
    if (mPseudonymisationRequired) {
      auto localPseudonym = globalConfig->getUserPseudonymFormat().makeUserPseudonym(mDescriptor.getParticipant().getLocalPseudonym());
      pseudonymiser = Pseudonymiser(mDescriptor.getExtra().at("pseudonymPlaceholder").plaintext(), localPseudonym);

      auto in = std::ifstream(mPath.string(), std::ios::binary);
      auto temppath = std::filesystem::path(mPath.string() + ".tmp");
      auto tempOut = std::ofstream(temppath.string(), std::ios::binary);

      // Hashing has been postponed, so do it now.
      auto writeAndHash = [&tempOut, &hasher = mHasher](const char* c, const std::streamsize l) {tempOut.write(c, l); hasher.update(c, static_cast<size_t>(l)); tempOut.flush();};
      pseudonymiser->pseudonymise(in, writeAndHash);

      in.close();
      tempOut.close();
      std::filesystem::remove(mPath);
      std::filesystem::rename(temppath, mPath);
    }

    hash = mHasher.digest();
    path = mPath;
  }

  try {
    mDestination->setStoredDataHash(mDescriptor, path, mFileName, hash);
  }
  catch (...) {
    LOG(LOG_TAG, error) << "Could not write stored data hash for record at " << mPath.string() << ": " << GetExceptionMessage(std::current_exception());
    throw;
  }
}

bool DownloadDirectory::RecordStorageStream::isCommitted() const noexcept
{
  return !mRaw;
}
