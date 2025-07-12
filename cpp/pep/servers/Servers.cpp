#include <pep/servers/Servers.hpp>

#include <pep/accessmanager/AccessManager.hpp>
#include <pep/keyserver/KeyServer.hpp>
#include <pep/registrationserver/RegistrationServer.hpp>
#include <pep/storagefacility/StorageFacility.hpp>
#include <pep/transcryptor/Transcryptor.hpp>
#include <pep/authserver/Authserver.hpp>

namespace pep {

void Servers::runAsync(const std::filesystem::path& configPath) {
  startService<StorageFacility>(configPath, "storagefacility/StorageFacility.json", "StorageFacility");
  startService<KeyServer>(configPath, "keyserver/KeyServer.json", "KeyServer");
  startService<Transcryptor>(configPath, "transcryptor/Transcryptor.json", "Transcryptor");
  startService<AccessManager>(configPath, "accessmanager/AccessManager.json", "AccessManager");
  startService<RegistrationServer>(configPath, "registrationserver/RegistrationServer.json", "RegistrationServer");
  startService<Authserver>(configPath, "authserver/Authserver.json", "Authserver");

  const std::lock_guard<std::mutex> lock(mIsRunningMutex);
  mIsRunning = true;
}

void Servers::wait() {
  std::unique_lock<std::mutex> lock{ mIsRunningMutex };
  mIsRunningCv.wait(lock, [this] { return !mIsRunning; });

  for (auto& service : mServices) {
    service->stop();
  }
  for (auto& thread : mThreads) {
    thread->join();
  }
}

void Servers::stop() {
  {
    const std::lock_guard<std::mutex> lock(mIsRunningMutex);
    mIsRunning = false;
  }
  mIsRunningCv.notify_all();
}

}
