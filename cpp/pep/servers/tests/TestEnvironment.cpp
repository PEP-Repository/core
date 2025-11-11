#include <pep/servers/Servers.hpp>
#include <pep/utils/RegisteredTestEnvironment.hpp>

namespace {

class ServerTestEnvironment : public pep::SelfRegisteringTestEnvironment<ServerTestEnvironment> {
private:
  pep::Servers servers;
public:
  ServerTestEnvironment(int argc, char* argv[]) //NOLINT(modernize-avoid-c-arrays)
    : pep::SelfRegisteringTestEnvironment<ServerTestEnvironment>(argc, argv) {
  }

  // Override this to define how to set up the environment.
  void SetUp() override {
    //clean-up any leftovers from a previous run (or files that have been copied over by cmake)
    std::filesystem::remove("serverstest/accessmanager/accessManagerStorage.sqlite");
    std::filesystem::remove("serverstest/authserver/Authserver.sqlite");
    std::filesystem::remove("serverstest/registrationserver/ShadowShortPseudonyms.sqlite");
    std::filesystem::remove("serverstest/transcryptor/transcryptorStorage.sqlite");
    std::filesystem::remove_all("serverstest/storagefacility/data");
    std::filesystem::remove_all("serverstest/storagefacility/meta");
    std::filesystem::create_directories("serverstest/storagefacility/data/myBucket");

    servers.runAsync("./serverstest");
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }

  // Override this to define how to tear down the environment.
  void TearDown() override {
    servers.stop();
    servers.wait();
  }
};

}
