#include <pep/application/CommandLineUtility.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/Log.hpp>
#include <pep/async/IOServiceThread.hpp>
#include <pep/client/Client.hpp>
#include <pep/httpserver/OAuthClient.hpp>
#include <pep/utils/ChronoUtil.hpp>
#include <pep/utils/Paths.hpp>
#include <pep/utils/WorkingDirectory.hpp>
#include <pep/auth/OAuthToken.hpp>

#include <cassert>

#include <boost/algorithm/string/predicate.hpp>

namespace {

const std::string LOG_TAG = "Logon utility";

}

class LogonApplication : public pep::commandline::Utility {

private:
  pep::Configuration mConfig;
  std::shared_ptr<boost::asio::io_service> mIoService;
  std::shared_ptr<boost::asio::io_service::work> mWork;
  std::shared_ptr<pep::Client> mClient;
  int mExitCode;
  bool mLongLived;

public:
  void OnLogonSuccess(const std::string& token) {
    mClient = pep::Client::OpenClient(mConfig, mIoService);

    auto keysFilePath = mClient->getKeysFilePath();

    if (keysFilePath.has_value() && !mLongLived) { // Client enrollment will write keys to file
      mClient->enrollUser(token)
        .subscribe(
          [keysFilePath = *keysFilePath](const pep::EnrollmentResult& result) {
            std::cout << "Wrote enrollment result (keys) to " << keysFilePath << std::endl;
          },
          [this](std::exception_ptr error) {
            LOG(LOG_TAG, pep::error) << "Enrollment failed: " << pep::GetExceptionMessage(error);
            this->stopEventLoop(EXIT_FAILURE);
          },
          [this]() {this->stopEventLoop(EXIT_SUCCESS); }
        );
    }
    else { // Keys cannot be written to file: write token instead
      auto tokenPath = pep::GetWorkingDirectory() / pep::OAuthToken::DEFAULT_JSON_FILE_NAME;
      pep::OAuthToken::Parse(token).writeJson(tokenPath);
      std::cout << "Wrote token to " << tokenPath << std::endl;
      this->stopEventLoop(EXIT_SUCCESS);
    }
  }

private:
  void stopEventLoop(int exitCode) {
    mExitCode = exitCode;
    mWork.reset();

    if (mClient == nullptr)
      return;

    mClient->shutdown().subscribe(
      [](pep::FakeVoid) {},
      [this](std::exception_ptr ep) {
        mClient->getIOservice()->stop();
    },
      [] {});
  };

  int execute() override {

    mConfig = this->loadMainConfigFile();
    auto& params = this->getParameterValues();

    mIoService = std::make_shared<boost::asio::io_service>();
    mWork = std::make_shared<boost::asio::io_service::work>(*mIoService);

    std::optional<std::chrono::seconds> validity = std::nullopt;
    std::string validityStr = params.get<std::string>("validity-duration");
    if(!boost::iequals(validityStr, "max")) {
      validity = pep::chrono::ParseDuration<std::chrono::seconds>(validityStr);
    }
    mLongLived = params.has("long-lived");

    bool limitedEnvironment = params.has("limited-environment");
    // We not only have a parameter for this, but also an environment variable. That way, we can change the default behaviour Docker images
    //NOLINTNEXTLINE(concurrency-mt-unsafe) std::getenv is thread safe as long as we do not setenv/unsetenv/putenv
    if(const char* limitedEnvironmentEnvVar = std::getenv("PEP_LOGON_LIMITED")) {
      limitedEnvironment |= std::string_view(limitedEnvironmentEnvVar) == "1";
    }

    auto oauth = pep::OAuthClient::Create(pep::OAuthClient::Parameters{
      .ioService = mIoService,
      .config = mConfig.get_child("AuthenticationServer"),
      .limitedEnvironment = limitedEnvironment,
      .longLived = mLongLived,
      .validityDuration = validity});

    boost::asio::post(*mIoService, [oauth, this]() {
      oauth->run().subscribe(
        [this](std::string token) {
          OnLogonSuccess(token);
        },
        [this](std::exception_ptr ep) {
          LOG(LOG_TAG, pep::error) << "Unexpected problem occurred: " + pep::GetExceptionMessage(ep);
          stopEventLoop(EXIT_FAILURE);
        }
      );
    });


    mIoService->run();
    mClient = nullptr;
    return mExitCode;
  }

protected:
  std::string getDescription() const override {
    return "Logs on to the PEP system";
  }

  pep::commandline::Parameters getSupportedParameters() const override {
    return pep::commandline::Utility::getSupportedParameters()
      + MakeConfigFileParameters(pep::GetResourceWorkingDirForOS(), "ClientConfig.json", true)
      + pep::commandline::Parameter("long-lived",
        "Request a long-lived authentication file, e.g. for use on a server, or in automated processes.").shorthand('l')
      + pep::commandline::Parameter("validity-duration", "If a long-lived authentication file is requested, it should be valid for the specified amount of time."
          "Use either 'max' or a numerical value with suffix d/day(s), h/hour(s), m/minute(s) or s/second(s)").shorthand('d').value(pep::commandline::Value<std::string>().defaultsTo("max"))
     + pep::commandline::Parameter("oauth-token-path", "Store the OAuthToken file to the specified location.").shorthand('o').value(pep::commandline::Value<std::filesystem::path>()
       .defaultsTo(pep::GetWorkingDirectory() / pep::OAuthToken::DEFAULT_JSON_FILE_NAME))
     + pep::commandline::Parameter("limited-environment", "Use this if you are running on a limited environment, e.g. a server."
                                                          " Can also be enabled by setting environment variable 'PEP_LOGON_LIMITED' to 1.");
  }
};

PEP_DEFINE_MAIN_FUNCTION(LogonApplication)
