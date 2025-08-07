#pragma once

#include <pep/service-application/Service.hpp>
#include <pep/utils/ThreadUtil.hpp>

#include <filesystem>
#include <memory>
#include <thread>
#include <vector>

namespace pep {

class Servers {
private:
  std::vector<std::shared_ptr<ServiceBase>> mServices;
  std::vector<std::shared_ptr<std::thread>> mThreads;
  std::condition_variable mIsRunningCv;
  std::mutex mIsRunningMutex; 
  bool mIsRunning = false;

  static void RunService(std::shared_ptr<ServiceBase> service, std::string name) {
    ThreadName::Set(name);
    service->run();
  }

public:
  template <typename TServer>
  void startService(std::filesystem::path rootConfig, const char* configurationFile, std::string name) {
    auto path = std::filesystem::absolute(rootConfig / configurationFile);
    auto config = Configuration::FromFile(path);
    std::shared_ptr<ServiceBase> service = std::make_shared<Service<TServer>>(std::move(config));
    mServices.push_back(service);
    auto thread = std::make_shared<std::thread>(&RunService, service, std::move(name));
    mThreads.push_back(thread);
  }

  void runAsync(const std::filesystem::path& configPath);
  void wait();
  void stop();
};

}
