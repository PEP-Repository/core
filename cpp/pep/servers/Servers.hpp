#pragma once

#include <vector>
#include <thread>
#include <memory>
#include <filesystem>
#include <pep/service-application/Service.hpp>

namespace pep {

class Servers {
private:
  std::vector<std::shared_ptr<ServiceBase>> mServices;
  std::vector<std::shared_ptr<std::thread>> mThreads;
  std::condition_variable mIsRunningCv;
  std::mutex mIsRunningMutex; 
  bool mIsRunning;

  static void RunService(std::shared_ptr<ServiceBase> service) {
    service->run();
  }

public:
  template <typename TServer>
  void startService(std::filesystem::path rootConfig, const char* configurationFile) {
    auto path = std::filesystem::absolute(rootConfig / configurationFile);
    std::shared_ptr<ServiceBase> service = std::make_shared<Service<TServer>>(path);
    mServices.push_back(service);
    auto thread = std::make_shared<std::thread>(&RunService, service);
    mThreads.push_back(thread);
  }

  void runAsync(const std::filesystem::path& configPath);
  void wait();
  void stop();
};

}
