#pragma once

#include <pep/client/Client.hpp>
#include <pep/application/CommandLineUtility.hpp>
#include <pep/async/FakeVoid.hpp>
#include <pep/auth/OAuthToken.hpp>

class CliApplication : public pep::commandline::Utility {
private:
  std::shared_ptr<pep::Client> mClient = nullptr;
  std::shared_ptr<boost::asio::io_service::work> mWork;
  std::optional<std::string> mRequiredGroup, mRequiredSubject;

protected:
  std::optional<pep::severity_level> consoleLogMinimumSeverityLevel() const override;
  std::string getDescription() const override;
  pep::commandline::Parameters getSupportedParameters() const override;
  inline std::optional<std::string> getRelativeDocumentationUrl() const override { return "Using-pepcli"; }
  void finalizeParameters() override;
  std::vector<std::shared_ptr<pep::commandline::Command>> createChildCommands() override;

private:
  std::optional<std::string> getTokenSecret() const;
  rxcpp::observable<pep::FakeVoid> connectClient(bool ensureEnrolled);

public:
  int executeEventLoopFor(bool ensureEnrolled, std::function<rxcpp::observable<pep::FakeVoid>(std::shared_ptr<pep::Client> client)> callback);
  std::optional<pep::OAuthToken> getTokenParameter() const;
};