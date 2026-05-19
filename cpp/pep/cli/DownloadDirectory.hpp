#pragma once

#include <pep/core-client/CoreClient_fwd.hpp>
#include <pep/utils/Shared.hpp>
#include <pep/cli/DownloadMetadata.hpp>
#include <pep/async/FakeVoid.hpp>
#include <pep/utils/Filesystem.hpp>
#include <pep/utils/SafePath.hpp>

#include <rxcpp/rx-lite.hpp>

#include <functional>
#include <vector>
#include <filesystem>

#include <xxhash.h>

namespace pep::cli {

class DownloadDirectory : public std::enable_shared_from_this<DownloadDirectory>, public SharedConstructor<DownloadDirectory> {
  friend class SharedConstructor<DownloadDirectory>;

public:
  static const bool APPLY_FILE_EXTENSIONS_BY_DEFAULT = true;

  struct ContentSpecification {
    std::vector<std::string> groups;
    std::vector<PolymorphicPseudonym> pps;
    std::vector<std::string> columnGroups;
    std::vector<std::string> columns;
  };

  struct Specification {
    ContentSpecification content;
    std::string accessGroup;
    bool applyFileExtensions = APPLY_FILE_EXTENSIONS_BY_DEFAULT;

    std::string toString() const;
    static Specification FromString(const std::string& value);
  };

  class RecordStorageStream {
    friend class DownloadDirectory;

  private:
    std::shared_ptr<DownloadDirectory> mDestination;
    RecordDescriptor mDescriptor;
    SafePath mPath;
    SafeFileName mFileName;
    size_t mFileSize;
    size_t mWritten = 0;
    std::shared_ptr<std::ofstream> mRaw;
    XxHasher mHasher;
    bool mPseudonymisationRequired{false};
    bool mArchiveExtractingRequired{false};

  private:
    explicit RecordStorageStream(std::shared_ptr<DownloadDirectory> destination, RecordDescriptor descriptor, SafePath path, bool pseudonymisationRequired, bool archiveExtractionRequired, size_t fileSize);

  public:
    // This class has reference semantics: prevent compiler from generating copy operations
    RecordStorageStream(const RecordStorageStream&) = delete;
    RecordStorageStream& operator =(const RecordStorageStream&) = delete;
    // The presence of the user-defined destructor should already prevent the compiler from generating move operations. But just to be sure...
    RecordStorageStream(RecordStorageStream&&) = delete;
    RecordStorageStream& operator =(RecordStorageStream&&) = delete;

    ~RecordStorageStream() noexcept;

    const RecordDescriptor& getRecordDescriptor() const noexcept { return mDescriptor; }

    SafePath getRelativePath() const;

    // Writes the given chunk to the stream. Automatically commits this stream once signaled filesize is reached.
    void write(const std::string& part, std::shared_ptr<GlobalConfiguration> globalConfig);

    /*\brief Completes the process of downloading a cell. Optional pseudonymisation or extracting of archived data is done here, after all network traffic is done.
     * The resulting file is hashed again and checked against a hash that was calculated during the writing of the file, to check for any errors in I/O.
     */
    void commit(std::shared_ptr<GlobalConfiguration> globalConfig);

    bool isCommitted() const noexcept;
  };

  struct PullOptions {
    bool assumePristine = false;

    PullOptions() noexcept = default; // Workaround for Clang build failure: see https://stackoverflow.com/a/44693603
  };

  struct NonPristineEntry {
    std::optional<RecordDescriptor> record;
    std::optional<std::filesystem::path> path;
  };

private:
  SafePath mRoot;
  bool mApplyFileExtensions = APPLY_FILE_EXTENSIONS_BY_DEFAULT;
  DownloadMetadata mMetadata;
  std::shared_ptr<GlobalConfiguration> mGlobalConfig;

  void setStoredDataHash(const RecordDescriptor& field, const SafePath& path, const SafeFileName& fileName, XxHasher::Hash hash);
  std::optional<XxHasher::Hash> getCurrentDataHash(const SafePath& path) const;
  std::optional<XxHasher::Hash> getCurrentDataHash(const RecordDescriptor& descriptor) const;

  std::vector<RecordDescriptor> getRecords(const std::function<bool(const RecordDescriptor&)>& match) const;

private:
  explicit DownloadDirectory(const std::filesystem::path& root, std::shared_ptr<GlobalConfiguration> globalConfig, const Progress::OnCreation& onCreateProgress); // Opens existing directory
  DownloadDirectory(const std::filesystem::path& root, std::shared_ptr<CoreClient> client, const ContentSpecification& content, std::shared_ptr<GlobalConfiguration> globalConfig, bool applyFileExtensions); // Initializes new directory

  std::optional<Specification> tryReadSpecification() const;
  SafePath provideParticipantDirectory(const ParticipantIdentifier& id);

  SafePath getDataStoragePath(const RecordDescriptor& descriptor);

  bool deleteRecord(const RecordDescriptor& descriptor);
  bool renameRecord(const RecordDescriptor& descriptor, const SafePath& newPath);

  void trackExistingPaths(filesystem::SetOfExistingPaths& dirs, filesystem::SetOfExistingPaths& files, const RecordDescriptor& descriptor) const;
  filesystem::SetOfExistingPaths getUnknownContents(const filesystem::SetOfExistingPaths& knownDirs, const filesystem::SetOfExistingPaths& knownFiles) const;

public:
  virtual ~DownloadDirectory() noexcept = default;

  std::vector<RecordDescriptor> list() const;
  std::vector<RecordDescriptor> list(const ParticipantIdentifier& id) const;
  std::vector<RecordDescriptor> list(const ParticipantIdentifier& id, const std::string& column) const;

  bool hasPristineData(const RecordDescriptor& descriptor) const;

  std::shared_ptr<RecordStorageStream> create(RecordDescriptor descriptor, bool pseudonymisationRequired, bool archiveExtractionRequired, size_t fileSize);
  bool remove(const RecordDescriptor& descriptor); // Would be called "delete" if that weren't a keyword
  bool update(const RecordDescriptor& descriptor, const RecordDescriptor& updated);

  rxcpp::observable<FakeVoid> pull(std::shared_ptr<CoreClient> source, const PullOptions& options, const Progress::OnCreation& onCreateProgress);

  const SafePath& getPath() const noexcept { return mRoot; }
  std::filesystem::path getSpecificationFilePath() const;
  SafePath getParticipantDirectory(const ParticipantIdentifier& id) const;
  std::optional<SafePath> getParticipantDirectoryIfExists(const ParticipantIdentifier& id) const;
  std::optional<SafePath> getRecordFileName(const RecordDescriptor& descriptor, bool absolute = true) const;

  std::vector<NonPristineEntry> getNonPristineEntries(const Progress::OnCreation& onCreateProgress) const;
  filesystem::SetOfExistingPaths getUnknownContents() const;

  Specification getSpecification() const;

  void clear();
};

}
