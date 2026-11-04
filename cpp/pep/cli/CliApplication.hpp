#pragma once

#include <pep/application/CommandLineCommand.hpp>
#include <pep/application/CommandLineParameter.hpp> // for commandline::Parameters
#include <pep/application/CommandLineUtility.hpp>
#include <pep/async/FakeVoid.hpp>
#include <pep/async/WorkGuard.hpp>
#include <pep/auth/OAuthToken.hpp>
#include <pep/client/Client_fwd.hpp>
#include <pep/utils/Log.hpp> // for Severity

#include <rxcpp/rx-lite.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace pep::cli {

extern const std::string LogTag;

class CliApplication : public pep::commandline::Utility {
private:
  std::shared_ptr<pep::Client> client_ = nullptr;
  std::unique_ptr<WorkGuard> workGuard_;
  std::optional<std::string> requiredGroup_, requiredSubject_;

protected:
  std::optional<pep::Severity> consoleLogMinimumSeverityLevel() const override;
  std::string getDescription() const override;
  pep::commandline::Parameters getSupportedParameters() const override;
  inline std::optional<std::string> getRelativeDocumentationUrl() const override { return "using-pepcli"; }
  void finalizeParameters() override;
  std::vector<std::shared_ptr<pep::commandline::Command>> createChildCommands() override;

private:
  std::optional<std::string> getTokenSecret() const;
  rxcpp::observable<pep::FakeVoid> connectClient(bool ensureEnrolled);

public:
  int executeEventLoopFor(bool ensureEnrolled, std::function<rxcpp::observable<pep::FakeVoid>(std::shared_ptr<pep::Client> client)> callback);
  std::optional<pep::OAuthToken> getTokenParameter() const;
};

}
