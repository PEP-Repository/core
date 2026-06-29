#include <pep/utils/RegisteredTestEnvironment.hpp>

#include <filesystem>
#include <string>
#include <thread>
#include <algorithm>

namespace {

class StorageFacilityTestEnvironment : public pep::SelfRegisteringTestEnvironment<StorageFacilityTestEnvironment> {
private:
  std::optional<std::filesystem::path> s3proxySh_;

  bool invokeS3proxySh(const char* command) {
    if (s3proxySh_.has_value()) {
      std::string cmd = s3proxySh_->string() + " " + command;
      //NOLINTNEXTLINE(concurrency-mt-unsafe,bugprone-command-processor) Tests run single-threaded; Only used in tests
      std::system(cmd.c_str());
      return true;
    }

    return false;
  }

public:
  StorageFacilityTestEnvironment(int argc, char* argv[]) //NOLINT(modernize-avoid-c-arrays)
    : pep::SelfRegisteringTestEnvironment<StorageFacilityTestEnvironment>(argc, argv) {
    auto end = argv + argc;
    if (std::find(argv, end, std::string("--launch-s3proxy")) != end) {
      s3proxySh_ = std::filesystem::path(argv[0]).parent_path() / "s3proxy.sh";
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
