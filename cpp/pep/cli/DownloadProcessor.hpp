#pragma once

#include <pep/cli/DownloadDirectory.hpp>

namespace pep {

class DownloadProcessor : public std::enable_shared_from_this<DownloadProcessor>, public SharedConstructor<DownloadProcessor> {
  friend class SharedConstructor<DownloadProcessor>;

private:
  std::shared_ptr<DownloadDirectory> mDestination;
  std::shared_ptr<GlobalConfiguration> mGlobalConfig;

  DownloadProcessor& operator =(const DownloadProcessor& other) = delete;

  std::shared_ptr<DownloadDirectory::RecordStorageStream> openStorageStream(const RecordDescriptor& descriptor, size_t fileSize, Progress& progress);

protected:
  void fail(const std::string& message);

public:
  explicit DownloadProcessor(std::shared_ptr<DownloadDirectory> destination, std::shared_ptr<GlobalConfiguration> globalConfig)
    : mDestination(destination), mGlobalConfig(globalConfig) {}
  virtual ~DownloadProcessor() noexcept {}

  rxcpp::observable<FakeVoid> update(std::shared_ptr<CoreClient> client, const DownloadDirectory::PullOptions& options, const Progress::OnCreation& onProgressCreation);
};

}
