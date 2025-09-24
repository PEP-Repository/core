#include <pep/application/CommandLineUtility.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/Log.hpp>
#include <pep/async/RxFinallyExhaust.hpp>
#include <pep/client/Client.hpp>
#include <pep/oauth-client/OAuthClient.hpp>
#include <pep/oauth-client/BrowserAuthorization.hpp>
#include <pep/oauth-client/ConsoleAuthorization.hpp>
#include <pep/utils/ChronoUtil.hpp>
#include <pep/utils/Paths.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/auth/OAuthToken.hpp>

#include <cassert>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>

namespace {

const std::string LOG_TAG = "Logon utility";

class LogonApplication : public pep::commandline::Utility {

private:
  pep::Configuration mConfig;
  std::shared_ptr<boost::asio::io_context> mIoContext;
  bool mLongLived = false;

  rxcpp::observable<pep::AuthorizationResult> authorize(const pep::commandline::NamedValues& params) {
    std::optional<std::chrono::seconds> validity = std::nullopt;
    std::string validityStr = params.get<std::string>("validity-duration");
    if (!boost::iequals(validityStr, "max")) {
      validity = pep::chrono::ParseDuration<std::chrono::seconds>(validityStr);
    }

    /// Whether we are running in a limited environment, i.e. we can't open a browser directly, and can't easily listen on localhost for a redirect from the browser. This is e.g. the case when running on a server, or via Docker.
    bool limitedEnvironment = params.has("limited-environment");
    // We not only have a parameter for this, but also an environment variable. That way, we can change the default behaviour Docker images
    //NOLINTNEXTLINE(concurrency-mt-unsafe) std::getenv is thread safe as long as we do not setenv/unsetenv/putenv
    if (const char* limitedEnvironmentEnvVar = std::getenv("PEP_LOGON_LIMITED")) {
      limitedEnvironment |= std::string_view(limitedEnvironmentEnvVar) == "1";
    }

    assert(mIoContext != nullptr);
    auto oauth = pep::OAuthClient::Create(pep::OAuthClient::Parameters{
      .io_context = mIoContext,
      .config = mConfig.get_child("AuthenticationServer"),
      .authorizationMethod = limitedEnvironment ? pep::ConsoleAuthorization : pep::BrowserAuthorization,
      .longLived = mLongLived,
      .validityDuration = validity });

    // Use a work guard to ensure that the I/O service doesn't terminate while we authenticate e.g. using BrowserAuthorization.
    // TODO: BrowserAuthorization should do this itself, making it more compatible with ConsoleAuthorization (which blocks until user has authorized)
    auto workGuard = pep::MakeSharedCopy(make_work_guard(*mIoContext));
    return oauth->run()
      .finally([workGuard]() { workGuard->reset(); });
  }

  rxcpp::observable<bool> handleAuthorizationResult(const pep::AuthorizationResult& auth) {
    if (!auth) {
      LOG(LOG_TAG, pep::error) << "Authorization failed: " + pep::GetExceptionMessage(auth.exception());
      return rxcpp::observable<>::just(false);
    }

    const auto& token = *auth;
    if (mLongLived) {
      return this->writeToken(token);
    }

    assert(mIoContext != nullptr);
    auto client = pep::Client::OpenClient(mConfig, mIoContext);
    return this->writeShortLived(token, client)
      .op(pep::RxFinallyExhaust(pep::observe_on_asio(*mIoContext), [client]() {
      return client->shutdown()
        .tap(
          [](pep::FakeVoid) { /* ignore*/ },
          [client](std::exception_ptr) {client->getIoContext()->stop(); } // TODO: Client class itself should un-schedule all its work if shutdown fails
        );
        }));
  }

  rxcpp::observable<bool> writeShortLived(const std::string& token, std::shared_ptr<pep::Client> client) {
    auto keysFilePath = client->getKeysFilePath();
    if (!keysFilePath.has_value()) {
      return this->writeToken(token);
    }

    return client->enrollUser(token) // Client enrollment will write keys to file
      .map([keysFilePath = *keysFilePath](const pep::EnrollmentResult& result)
        {
          std::cout << "Wrote enrollment result (keys) to " << keysFilePath << std::endl;
          return true;
        })
      .on_error_resume_next([](std::exception_ptr error) // Don't let the application report an **unexpected** problem
        {
          LOG(LOG_TAG, pep::error) << "Enrollment failed: " << pep::GetExceptionMessage(error);
          return rxcpp::observable<>::just(false);
        });
  }

  rxcpp::observable<bool> writeToken(const std::string& token) {
    auto tokenPath = std::filesystem::current_path() / pep::OAuthToken::DEFAULT_JSON_FILE_NAME;
    pep::OAuthToken::Parse(token).writeJson(tokenPath);
    std::cout << "Wrote token to " << tokenPath << std::endl;
    return rxcpp::observable<>::just(true);
  }

  int execute() override {
    mConfig = this->loadMainConfigFile();
    auto& params = this->getParameterValues();

    mIoContext = std::make_shared<boost::asio::io_context>();
    mLongLived = params.has("long-lived");

    int exitCode = EXIT_FAILURE;
    this->authorize(params)
      .concat_map([this](const pep::AuthorizationResult& result) { return this->handleAuthorizationResult(result); })
      .subscribe(
        [&exitCode](bool success) {
          if (success) {
            exitCode = EXIT_SUCCESS;
          }
        },
        [](std::exception_ptr ep) {
          LOG(LOG_TAG, pep::error) << "Unexpected problem occurred: " + pep::GetExceptionMessage(ep);
        }
      );

    mIoContext->run();
    return exitCode;
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
       .defaultsTo(pep::OAuthToken::DEFAULT_JSON_FILE_NAME))
     + pep::commandline::Parameter("limited-environment", "Use this if you are running on a limited environment, e.g. a server."
                                                          " Can also be enabled by setting environment variable 'PEP_LOGON_LIMITED' to 1.");
  }
};

}

PEP_DEFINE_MAIN_FUNCTION(LogonApplication)
