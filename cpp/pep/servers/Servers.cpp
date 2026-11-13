#include <pep/servers/Servers.hpp>

#include <pep/accessmanager/AccessManager.hpp>
#include <pep/authserver/Authserver.hpp>
#include <pep/keyserver/KeyServer.hpp>
#include <pep/registrationserver/RegistrationServer.hpp>
#include <pep/server/NetworkedServer.hpp>
#include <pep/storagefacility/StorageFacility.hpp>
#include <pep/transcryptor/Transcryptor.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/ThreadUtil.hpp>

#include <boost/core/demangle.hpp>

#include <exception>
#include <filesystem>
#include <string>
#include <thread>
#include <typeinfo>
#include <vector>

namespace pep {

namespace {

const std::string LogTag = "Servers";

template <typename TServer>
NetworkedServer MakeServer(const std::filesystem::path& configFile) {
  try {
    PEP_LOG(LogTag, Severity::Info) << "Starting " << boost::core::demangle(typeid(TServer).name()) << " from " << configFile;
    return NetworkedServer::Make<TServer>(Configuration::FromFile(configFile));
  }
  catch (const std::exception& e) {
    PEP_LOG(LogTag, Severity::Critical) << "Failed to start server from " << configFile << ": " << e.what();
    throw;
  }
}

}

Servers::Servers(const std::filesystem::path& configDir) {
  auto dir = std::filesystem::absolute(configDir);
  instances_.push_back(MakeServer<StorageFacility>(dir / "storagefacility" / "StorageFacility.json"));
  instances_.push_back(MakeServer<KeyServer>(dir / "keyserver" / "KeyServer.json"));
  instances_.push_back(MakeServer<Transcryptor>(dir / "transcryptor" / "Transcryptor.json"));
  instances_.push_back(MakeServer<AccessManager>(dir / "accessmanager" / "AccessManager.json"));
  instances_.push_back(MakeServer<RegistrationServer>(dir / "registrationserver" / "RegistrationServer.json"));
  instances_.push_back(MakeServer<Authserver>(dir / "authserver" / "Authserver.json"));
}

void Servers::run() {
  std::vector<std::jthread> threads; // Joined on scope exit, i.e. when all servers have stopped
  threads.reserve(instances_.size());
  for (auto& server : instances_) {
    threads.emplace_back([&server] {
      ThreadName::Set(server.describe());
      server.start();
      });
  }
}

void Servers::stop() {
  for (auto& server : instances_) {
    server.stop();
  }
}

}
