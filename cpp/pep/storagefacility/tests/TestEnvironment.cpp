#include <pep/utils/RegisteredTestEnvironment.hpp>

#include <filesystem>
#include <string>
#include <thread>
#include <algorithm>


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
  StorageFacilityTestEnvironment(int argc, char* argv[])
    : pep::SelfRegisteringTestEnvironment<StorageFacilityTestEnvironment>(argc, argv) {
    auto end = argv + argc;
    if (std::find(argv, end, std::string("--launch-s3proxy")) != end) {
      mS3proxySh = std::filesystem::path(argv[0]).parent_path() / "s3proxy.sh";
    }
  }

  ~StorageFacilityTestEnvironment() override {}

  void SetUp() override {
    if (invokeS3proxySh("start")) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  }

  void TearDown() override {
    invokeS3proxySh("stop");
  }
};
