#pragma once

#include <pep/server/NetworkedServer.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/ThreadUtil.hpp>

#include <filesystem>
#include <memory>
#include <thread>
#include <vector>

namespace pep {

class Servers {
private:
  std::vector<std::shared_ptr<NetworkedServer>> mInstances;
  std::vector<std::shared_ptr<std::thread>> mThreads;
  std::condition_variable mIsRunningCv;
  std::mutex mIsRunningMutex; 
  bool mIsRunning = false;

  static void RunServer(std::shared_ptr<NetworkedServer> server) {
    ThreadName::Set(server->describe());
    server->start();
  }

  template <typename TServer>
  void startServer(std::filesystem::path rootConfig, const char* configurationFile) {
    try {
      auto path = std::filesystem::absolute(rootConfig / configurationFile);
      auto config = Configuration::FromFile(path);
      auto server = mInstances.emplace_back(MakeSharedCopy(NetworkedServer::Make<TServer>(config)));
      auto thread = std::make_shared<std::thread>(&RunServer, server);
      mThreads.push_back(thread);
    }
    catch (const std::exception& e) {
      LOG("Servers", critical) << "Failed to start server from " << configurationFile << ": " << e.what();
      throw;
    }
  }

public:
  
  void runAsync(const std::filesystem::path& configPath);
  void wait();
  void stop();
};

}
