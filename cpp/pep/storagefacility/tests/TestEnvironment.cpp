#include <pep/utils/RegisteredTestEnvironment.hpp>

#include <filesystem>
#include <string>
#include <thread>
#include <algorithm>

namespace {

class StorageFacilityTestEnvironment : public pep::SelfRegisteringTestEnvironment<StorageFacilityTestEnvironment> {
private:
  std::optional<std::filesystem::path> mS3proxySh;

  bool invokeS3proxySh(const char* command) {
    if (mS3proxySh.has_value()) {
      std::string cmd = mS3proxySh->string() + " " + command;
      std::system(cmd.c_str()); //NOLINT(concurrency-mt-unsafe) Tests run single-threaded
      return true;
    }

    return false;
  }

public:
  StorageFacilityTestEnvironment(int argc, char* argv[]) //NOLINT(modernize-avoid-c-arrays)
    : pep::SelfRegisteringTestEnvironment<StorageFacilityTestEnvironment>(argc, argv) {
    auto end = argv + argc;
    if (std::find(argv, end, std::string("--launch-s3proxy")) != end) {
      mS3proxySh = std::filesystem::path(argv[0]).parent_path() / "s3proxy.sh";
    }
  }

  void SetUp() override {
    if (invokeS3proxySh("start")) {
      // Give containers time to initialize. A single second is too short:
      // nginx then often produces "502 Bad Gateway" on my machine
      std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
  }

  void TearDown() override {
    invokeS3proxySh("stop");
  }
};

}
