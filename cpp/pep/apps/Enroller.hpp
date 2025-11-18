#pragma once

#include <pep/apps/Enrollment.hpp>
#include <pep/client/Client.hpp>
#include <pep/auth/EnrolledParty.hpp>

namespace pep {

class Enroller : public commandline::ChildCommandOf<EnrollmentApplication> {
protected:
  virtual std::vector<commandline::Parameter> getAuthorizationParameters() const = 0;
  virtual bool producesExtendedProperties() const noexcept = 0;
  virtual bool producesDataKey() const noexcept = 0;
  virtual rxcpp::observable<EnrollmentResult> enroll(std::shared_ptr<Client> client) const = 0;

  virtual void setProperties(Client::Builder& builder, const Configuration& config) const;
  virtual EndPoint getAccessManagerEndPoint(const Configuration& config) const;

  commandline::Parameters getSupportedParameters() const override {
    return commandline::ChildCommandOf<EnrollmentApplication>::getSupportedParameters()
      + this->getAuthorizationParameters()
      + commandline::Parameter("output-path", "Location of output file").value(commandline::Value<std::filesystem::path>().positional());
  }

  Enroller(EnrolledParty type, const std::string& description, EnrollmentApplication& parent)
    : commandline::ChildCommandOf<EnrollmentApplication>(
        std::to_string(static_cast<unsigned>(type)), "Enrolls " + description, parent)
  {}

  int execute() override;
};

class UserEnroller : public Enroller {
protected:
  std::vector<commandline::Parameter> getAuthorizationParameters() const override {
    return {
      commandline::Parameter("oauth-token", "OAuth token to use for enrollment").value(commandline::Value<std::string>().positional().required())
    };
  }
  inline bool producesExtendedProperties() const noexcept override { return true; }
  inline bool producesDataKey() const noexcept override { return true; }
  rxcpp::observable<EnrollmentResult> enroll(std::shared_ptr<Client> client) const override;

public:
  explicit UserEnroller(EnrollmentApplication& parent)
    : Enroller(EnrolledParty::User, "a user", parent) {
  }
};

class ServiceEnroller : public Enroller {
private:
  EnrolledParty mParty;
  bool mProducesDataKey;

protected:
  std::vector<commandline::Parameter> getAuthorizationParameters() const override {
    return {
      commandline::Parameter("private-key-file", "Path to file containing private key").value(commandline::Value<std::filesystem::path>().positional().required()),
      commandline::Parameter("certificate-file", "Path to file containing certificate").value(commandline::Value<std::filesystem::path>().positional().required())
    };
  }
  inline bool producesExtendedProperties() const noexcept override { return false; }
  inline bool producesDataKey() const noexcept override { return mProducesDataKey; }
  void setProperties(Client::Builder& builder, const Configuration& config) const override;
  EndPoint getAccessManagerEndPoint(const Configuration& config) const override;
  inline rxcpp::observable<EnrollmentResult> enroll(std::shared_ptr<Client> client) const override { return client->enrollServer(); }

public:
  ServiceEnroller(EnrolledParty party, const std::string& description, EnrollmentApplication& parent, bool producesDataKey = false)
    : Enroller(party, description, parent), mParty(party), mProducesDataKey(producesDataKey) {
  }
};

}
