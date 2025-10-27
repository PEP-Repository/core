#pragma once

#include <pep/accessmanager/AccessManagerMessages.hpp>
#include <pep/cli/DownloadDirectory.hpp>
#include <pep/utils/VectorOfVectors.hpp>

namespace pep::cli {

class DownloadProcessor : public std::enable_shared_from_this<DownloadProcessor>, public SharedConstructor<DownloadProcessor> {
  friend class SharedConstructor<DownloadProcessor>;

private:
  std::shared_ptr<DownloadDirectory> mDestination;
  std::shared_ptr<GlobalConfiguration> mGlobalConfig;

  std::shared_ptr<DownloadDirectory::RecordStorageStream> openStorageStream(RecordDescriptor descriptor, size_t fileSize, Progress& progress);

  struct Context;
  rxcpp::observable<std::shared_ptr<IndexedTicket2>> requestTicket(std::shared_ptr<Progress> progress, std::shared_ptr<Context> ctx);
  rxcpp::observable<std::shared_ptr<VectorOfVectors<std::shared_ptr<EnumerateResult>>>> listFiles(std::shared_ptr<Progress> progress, std::shared_ptr<Context> ctx);
  rxcpp::observable<std::shared_ptr<std::unordered_map<RecordDescriptor, std::shared_ptr<EnumerateResult>>>> locateFileContents(std::shared_ptr<Progress> progress, std::shared_ptr<Context> ctx, std::shared_ptr<VectorOfVectors<std::shared_ptr<EnumerateResult>>> metas);
  void prepareLocalData(std::shared_ptr<Progress> progress, std::shared_ptr<std::unordered_map<RecordDescriptor, std::shared_ptr<EnumerateResult>>> downloads, bool assumePristine);
  rxcpp::observable<FakeVoid> retrieveFromServer(std::shared_ptr<Progress> progress, std::shared_ptr<Context> ctx, std::shared_ptr<std::unordered_map<RecordDescriptor, std::shared_ptr<EnumerateResult>>> downloads);
  void processDataChunk(std::shared_ptr<Progress> retrieveProgress, std::shared_ptr<Context> ctx, const RetrievePage& result);

protected:
  [[noreturn]] void fail(const std::string& message);

public:
  explicit DownloadProcessor(std::shared_ptr<DownloadDirectory> destination, std::shared_ptr<GlobalConfiguration> globalConfig)
    : mDestination(destination), mGlobalConfig(globalConfig) {}
  DownloadProcessor& operator =(const DownloadProcessor& other) = delete;
  virtual ~DownloadProcessor() noexcept = default;

  rxcpp::observable<FakeVoid> update(std::shared_ptr<CoreClient> client, const DownloadDirectory::PullOptions& options, const Progress::OnCreation& onProgressCreation);
};

}
