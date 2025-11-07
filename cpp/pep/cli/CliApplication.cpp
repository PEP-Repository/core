#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/auth/UserGroup.hpp>
#include <pep/client/Client.hpp>
#include <pep/utils/Exceptions.hpp>

#include <iostream>
#include <ranges>
#include <string>
#include <vector>

#include <boost/algorithm/hex.hpp>
#include <boost/asio/io_context.hpp>

#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>

using namespace std::chrono;

namespace pep::cli {

const std::string LOG_TAG = "Cli";

std::optional<pep::severity_level> CliApplication::consoleLogMinimumSeverityLevel() const {
  return pep::severity_level::info;
}

std::string CliApplication::getDescription() const {
  return "Command line interface for PEP";
}

pep::commandline::Parameters CliApplication::getSupportedParameters() const {
  const auto daySecs = seconds{days{1}}.count();
  return pep::commandline::Utility::getSupportedParameters()
    + MakeConfigFileParameters(pep::GetResourceWorkingDirForOS(), "ClientConfig.json", false, "client-config-name", "client-working-directory") // Maintain backward compatible "--client-XYZ" switch names as aliases
    + pep::commandline::Parameter("oauth-token", "OAuth token to use to enroll. Accepts a token-string, JSON token or path to a token-file.").value(pep::commandline::Value<std::string>())
    + pep::commandline::Parameter("oauth-token-secret", "OAuth token secret to generate oauth token").value(pep::commandline::Value<std::string>())
    + pep::commandline::Parameter("oauth-token-duration", "Validity of generated token in seconds").value(pep::commandline::Value<seconds::rep>().defaultsTo(daySecs, "a day"))
    + pep::commandline::Parameter("oauth-token-subject", "Subject for generated token").value(pep::commandline::Value<std::string>().defaultsTo("pepcli"))
    + pep::commandline::Parameter("oauth-token-group", "Group for generated token").value(pep::commandline::Value<std::string>().defaultsTo(pep::UserGroup::ResearchAssessor));
}

void CliApplication::finalizeParameters() {
  // Get explicitly specified parameter values (before defaults are applied by base class's finalizeParameters)
  const auto& values = this->getParameterValues();
  if (values.has("oauth-token-subject")) {
    mRequiredSubject = values.get<std::string>("oauth-token-subject");
  }
  if (values.has("oauth-token-group")) {
    mRequiredGroup = values.get<std::string>("oauth-token-group");
  }

  pep::commandline::Utility::finalizeParameters();
}

rxcpp::observable<pep::FakeVoid> CliApplication::connectClient(bool ensureEnrolled) {
  if (mClient != nullptr) {
    assert(mWorkGuard != nullptr);
    return rxcpp::observable<>::just(pep::FakeVoid());
  }

  const auto& parameterValues = this->getParameterValues();
  pep::Configuration config = this->loadMainConfigFile();

  // Set up event loop
  auto io_context = std::make_shared<boost::asio::io_context>();

  // Start client
  auto client = pep::Client::OpenClient(
    config,
    io_context);

  // Whether we need to (re-)enroll
  auto enroll = ensureEnrolled;
  std::optional<pep::OAuthToken> token; // The token to enroll with

  auto cmdlineGroup = parameterValues.get<std::string>("oauth-token-group");
  auto cmdlineSubject = parameterValues.get<std::string>("oauth-token-subject");

  // ----- Situation 1: OAuth token provided on command line -----
  token = this->getTokenParameter();
  if (token != std::nullopt) {
    LOG(LOG_TAG, pep::info) << "Enrolling using provided OAuth token" << std::endl;
    enroll = true;
    if (!token->verify(mRequiredSubject, mRequiredGroup)) {
      if (mRequiredSubject && mRequiredGroup)
        throw std::runtime_error("Provided token not usable, expected subject: " + *mRequiredSubject + "and group: " + *mRequiredGroup);
      else
        throw std::runtime_error("Provided token not usable");
    }
  }

  // ----- Situation 2: re-use existing enrollment (ClientKeys) -----
  else {
    if (client->getEnrolled()) { // Ensure we don't provide a client with an incorrect enrollment, even if "ensureEnrolled" is false
      enroll = false;

      auto enrolledGroup = client->getEnrolledGroup();
      auto enrolledSubject = client->getEnrolledUser();

      if (mRequiredGroup && (enrolledGroup != *mRequiredGroup)) {
        LOG(LOG_TAG, pep::info) << "Enrolled for wrong group (" << enrolledGroup << ")" << std::endl;
        enroll = true;
      }
      if (mRequiredSubject && (enrolledSubject != *mRequiredSubject)) {
        LOG(LOG_TAG, pep::info) << "Enrolled as wrong user (" << enrolledSubject << ")" << std::endl;
        enroll = true;
      }
    } else if (enroll) {
      LOG(LOG_TAG, pep::info) << "Not enrolled or certificate expired." << std::endl;
    }
  }

  // ----- Situation 3: no (new) enrollment needed -----
  rxcpp::observable<pep::FakeVoid> result = rxcpp::observable<>::just(pep::FakeVoid());

  if (enroll) {
    auto tokenPath = pep::OAuthToken::DEFAULT_JSON_FILE_NAME;

    // ----- Situation 4: cached token -----
    if (!token) {
      if (std::filesystem::exists(tokenPath)) {
        LOG(LOG_TAG, pep::info)
          << "Cached token found in"
          << std::filesystem::canonical(tokenPath) << std::endl;
        token = pep::OAuthToken::ReadJson(tokenPath);
        if (!token->verify(mRequiredSubject, mRequiredGroup)) {
          LOG(LOG_TAG, pep::info) << "Not using cached token because it did not pass verification";
          token = std::nullopt;
        }
      }
    }

    // ----- Situation 5: generate token -----
    if (!token) {
      auto tokenSecret = this->getTokenSecret();
      if (tokenSecret.has_value()) {
        const auto now = TimeNow<sys_seconds>();
        const seconds oauthTokenDuration{parameterValues.get<seconds::rep>("oauth-token-duration")};

        LOG(LOG_TAG, pep::info) << " generated new token using oauth token secret" << std::endl;
        token = pep::OAuthToken::Generate(
          *tokenSecret,
          cmdlineSubject,
          cmdlineGroup,
          now,
          now + oauthTokenDuration);

        token->writeJson(tokenPath);
      }
    }

    // ----- We've exhausted all possibilities to provide a token: enroll now -----
    if (!token) {
      std::string message = "Please run pepLogon or specify --oauth-token";
      if (!ensureEnrolled) {
        message += ", or remove existing enrollment data";
      }
      throw std::runtime_error(message);
    }

    result = client->enrollUser(
      token->getSerializedForm()
    ).map([](pep::EnrollmentResult result) {
      LOG(LOG_TAG, pep::info) << " completed enrollment!" << std::endl;
      return pep::FakeVoid();
      });
  }

  mWorkGuard = std::make_unique<WorkGuard>(*io_context);
  mClient = client;

  return result;
}

std::vector<std::shared_ptr<pep::commandline::Command>> CliApplication::createChildCommands() {
  const auto commands = std::vector<std::shared_ptr<pep::commandline::Command>>{
      CreateCommandList(*this),
      CreateCommandGet(*this),
      CreateCommandStore(*this),
      CreateCommandDelete(*this),
      CreateCommandPull(*this),
      CreateCommandExport(*this),
      CreateCommandAma(*this),
      CreateCommandUser(*this),
      CreateCommandPing(*this),
      CreateCommandValidate(*this),
      CreateCommandVerifiers(*this),
      CreateCommandCastor(*this),
      CreateCommandMetrics(*this),
      CreateCommandRegister(*this),
      CreateCommandXEntry(*this),
      CreateCommandQuery(*this),
      CreateCommandHistory(*this),
      CreateCommandFileExtension(*this),
      CreateCommandToken(*this),
      CreateNoLongerSupportedCommand(*this, "asa", "Use 'user' or 'token' instead."),
      CreateCommandStructureMetadata(*this),
  };
  assert(std::ranges::none_of(commands, [](auto& ptr) { return ptr == nullptr; }));
  return commands;
}

int CliApplication::executeEventLoopFor(bool ensureEnrolled, std::function<rxcpp::observable<pep::FakeVoid>(std::shared_ptr<pep::Client> client)> callback) {
  int result;

  auto stopEventLoop = [this, &result](int exitCode) {
    result = exitCode;
    mWorkGuard.reset();

    if (mClient == nullptr)
      return;

    mClient->shutdown().subscribe(
      [](pep::FakeVoid) {},
      [this](std::exception_ptr ep) {
        mClient->getIoContext()->stop();
        LOG(LOG_TAG, pep::error) << "Unexpected problem shutting down SSL streams: " + pep::GetExceptionMessage(ep) << " | Forcefully shutting down.";
      },
      [] {});
    };

  connectClient(ensureEnrolled)
    .flat_map([this, callback](pep::FakeVoid unused) {
    return callback(mClient);
      }).subscribe(
        [](pep::FakeVoid) { /* ignore */ },
        [stopEventLoop](std::exception_ptr ep) {
          LOG(LOG_TAG, pep::error) << "error: " << pep::GetExceptionMessage(ep) << std::endl;
          stopEventLoop(4);
        },
        [stopEventLoop]() {stopEventLoop(0); }
      );

      assert(mClient != nullptr);

      // io_context.run() usually returns when there is no work to do.
      // Our this->mWorkGuard prevents this though.
      mClient->getIoContext()->run();
      mClient = nullptr;
      return result;
}

std::optional<std::string> CliApplication::getTokenSecret() const {
  const auto& parameterValues = this->getParameterValues();
  if (!parameterValues.has("oauth-token-secret")) {
    return std::nullopt;
  }

  auto tokenSecret = parameterValues.get<std::string>("oauth-token-secret");
  try {
    // Try to interpret the command line parameter as a hex value
    return boost::algorithm::unhex(tokenSecret);
  } catch (const boost::algorithm::non_hex_input&) {
    // Try to interpret the command line parameter as a path (to a .JSON file)
    if (!std::filesystem::exists(tokenSecret)) {
      throw std::runtime_error("Unusable OAuth token secret provided: it was not in a hex format and did not specify an existing file");
    }
  }

  pep::Configuration root = pep::Configuration::FromFile(tokenSecret);
  std::string secret = boost::algorithm::unhex(root.get<std::string>("OAuthTokenSecret"));

  LOG(LOG_TAG, pep::info)
    << " found oauth token secret in "
    << std::filesystem::canonical(tokenSecret) << std::endl;

  return secret;

}

std::optional<pep::OAuthToken> CliApplication::getTokenParameter() const {
  std::optional<pep::OAuthToken> token;

  const auto& parameterValues = this->getParameterValues();
  if (parameterValues.has("oauth-token")) {
    auto providedToken = parameterValues.get<std::string>("oauth-token");

    boost::system::error_code ec;
    bool exists = std::filesystem::exists(providedToken, ec);

    if (exists && !ec) {
      // Try parsing provided OAuth token as file
      try {
        token = pep::OAuthToken::ReadJson(providedToken);
      } catch (const std::runtime_error& e) {
        // Incorrect file format
        std::string errorMessage = "Parsing token as a file. Error while parsing JSON: ";
        errorMessage.append(e.what());
        throw std::runtime_error(errorMessage);
      }
    } else {
      // Try parsing provided OAuth token as string
      try {
        token = pep::OAuthToken::Parse(providedToken);
      } catch (const std::runtime_error& e) {
        // Not a file and not a token
        std::string errorMessage = "Token could not be parsed as a file: ";
        errorMessage.append(ec.message() + std::string(". Trying to parse as a token directly. Error: ") + e.what());
        throw std::runtime_error(errorMessage);
      }
    }
  }

  return token;
}

} // End namespace pep::cli

PEP_DEFINE_MAIN_FUNCTION(pep::cli::CliApplication)
