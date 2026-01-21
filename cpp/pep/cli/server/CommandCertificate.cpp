#include <regex>
#include <boost/log/keywords/target.hpp>
#include <pep/cli/Command.hpp>
#include <pep/client/Client.hpp>
#include <pep/cli/server/CommandCertificate.hpp>
#include <pep/utils/File.hpp>
#include <rxcpp/operators/rx-concat_map.hpp>

using namespace pep::cli;

namespace {
pep::commandline::Parameters MakeCommonSupportedParameters(bool isInput, std::string_view what, std::string_view extension) {
  auto traits = pep::ServerTraits::Where([](const pep::ServerTraits& traits){ return traits.hasSigningIdentity(); });

  auto ids = pep::RangeToVector(traits
    | std::views::transform(&pep::ServerTraits::commandLineId));
  std::ranges::sort(ids);

  return pep::commandline::Parameters()
    + pep::commandline::Parameter("server", "Restrict to specified server(s)").value(pep::commandline::Value<std::string>().multiple()
      .allow(ids))
    + pep::commandline::Parameter(isInput ? "input-file" : "output-file",
      std::format("Filename to {} the {}(s) {}. Can only be specified if you pass a single --server parameter. Default: PEP<ExpectedCommonName>.{}",
        isInput ? "read" : "write", what, isInput ? "from" : "to", extension))
      .value(pep::commandline::Value<std::filesystem::path>())
      .shorthand(isInput ? 'i' : 'o' )
    + pep::commandline::Parameter(isInput ? "input-directory" : "output-directory",
      std::format("Directory to {} the {}(s) {}. Can not be used together with --output-file. Directory must exist. Default: current working directory",
        isInput ? "read" : "write", what, isInput ? "from" : "to"))
      .value(pep::commandline::Value<std::filesystem::path>())
      .shorthand(isInput ? 'I' : 'O');
}

struct commonParams {
  std::vector<std::string> servers;
  std::optional<std::filesystem::path> targetFile;
  std::optional<std::filesystem::path> targetDirectory;

  commonParams(bool isInput, const pep::commandline::NamedValues& parameterValues) {
    std::string fileSwitch = isInput ? "input-file" : "output-file";
    std::string directorySwitch = isInput ? "input-directory" : "output-directory";

    this->servers = parameterValues.getOptionalMultiple<std::string>("server");
    this->targetFile = parameterValues.getOptional<std::filesystem::path>(fileSwitch);
    if (this->targetFile && this->servers.size() != 1) {
      throw std::runtime_error(std::format("--{} can only be used in combination with a single --server", fileSwitch));
    }
    this->targetDirectory = parameterValues.getOptional<std::filesystem::path>(directorySwitch);
    if (this->targetFile && this->targetDirectory) {
      throw std::runtime_error(std::format("--{} cannot be used together with --{}", fileSwitch, directorySwitch));
    }
  }
};

using signingServerAction = std::function<rxcpp::observable<pep::FakeVoid>(const pep::SigningServerProxy&, const std::filesystem::path&)>;
auto EventLoopCallBack(const commonParams& params, std::string_view extension, signingServerAction action) {
  return [params, extension, action](std::shared_ptr<pep::Client> client) {
    std::unordered_set<pep::ServerTraits> traits;

    if (params.servers.empty()) {
      traits = pep::ServerTraits::Where([](const pep::ServerTraits& traits){ return traits.hasSigningIdentity(); });
    }
    else {
      traits = pep::ServerTraits::Where([params](const pep::ServerTraits& traits){ return std::ranges::find(params.servers, traits.commandLineId()) != params.servers.end(); });
    }

    return rxcpp::rxs::iterate(traits)
      .map([client](const pep::ServerTraits& traits) {
        assert(traits.hasSigningIdentity());
        return std::static_pointer_cast<const pep::SigningServerProxy>(client->getServerProxy(traits));
      })
      .concat_map([params, extension, action](std::shared_ptr<const pep::SigningServerProxy> proxy) {
        std::filesystem::path targetFilePath;
        if (params.targetFile) {
          targetFilePath = *params.targetFile;
        }
        else {
          if (std::ranges::any_of(proxy->getExpectedCommonName(), boost::is_any_of(R"("*/:<>?\|)") || boost::is_from_range('\0', '\x1F'))) {
            throw std::runtime_error("Expected common name contains characters that are not allowed in filenames on some systems. Can't autodeduce target filename");
          }
          targetFilePath = params.targetDirectory.value_or(".") / std::format("PEP{}.{}", proxy->getExpectedCommonName(), extension);
        }
        return action(*proxy, targetFilePath);
      });
  };
}
}

class CommandServer::CommandCertificate::CommandRequestCSR : public ChildCommandOf<CommandCertificate> {
public:
  CommandRequestCSR(CommandCertificate& parent)
  : ChildCommandOf<CommandCertificate>("request-csr", "Request one or more servers to generate a new signing private key, and create a certificate signing request for that.", parent) {}

protected:
  pep::commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf<CommandCertificate>::getSupportedParameters()
      + MakeCommonSupportedParameters(false, "certificate signing request", "csr");
  }

  int execute() override {
    commonParams params(false, this->getParameterValues());

    return this->executeEventLoopFor(EventLoopCallBack(params, "csr", [](const SigningServerProxy& proxy, const std::filesystem::path& targetPath) -> rxcpp::observable<FakeVoid> {
      return proxy.requestCertificateSigningRequest().map([targetPath](const X509CertificateSigningRequest& csr) {
            WriteFile(targetPath, csr.toPem());
            LOG(LOG_TAG, info) << "CSR is saved to " << targetPath;
            return FakeVoid();
          });
    }));
  }
};

class CommandServer::CommandCertificate::CommandReplace : public ChildCommandOf<CommandCertificate> {
public:
  CommandReplace(CommandCertificate& parent)
  : ChildCommandOf<CommandCertificate>("replace", "Replace the signing certificate that is currently in use at one or more servers. Do not write it back to the filesystem yet. Use the 'commit' command for that.", parent) {}
protected:
  pep::commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf<CommandCertificate>::getSupportedParameters()
      + MakeCommonSupportedParameters(true, "certificate chain", "chain")
      + pep::commandline::Parameter("force", "Force the certificate to be replaced, even when the subject is different than that of the current certificate.").shorthand('f');
  }

  int execute() override {
    commonParams params(true, this->getParameterValues());
    bool force = this->getParameterValues().has("force");

    return this->executeEventLoopFor(EventLoopCallBack(params, "chain", [force](const SigningServerProxy& proxy, const std::filesystem::path& targetPath) -> rxcpp::observable<FakeVoid> {
      X509CertificateChain chain(X509CertificatesFromPem(ReadFile(targetPath)));
      return proxy.requestCertificateReplacement(chain, force);
    }));
  }
};

class CommandServer::CommandCertificate::CommandCommit : public ChildCommandOf<CommandCertificate> {
public:
  CommandCommit(CommandCertificate& parent)
  : ChildCommandOf<CommandCertificate>("commit", "Commit certificates to disk, that were previously deployed to one or more servers with the 'replace' command. This permanently replaces the previous certificates.", parent) {}
protected:
  pep::commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf<CommandCertificate>::getSupportedParameters()
      + MakeCommonSupportedParameters(true, "certificate chain", "chain");
  }

  int execute() override {
    commonParams params(true, this->getParameterValues());

    return this->executeEventLoopFor(EventLoopCallBack(params, "chain", [](const SigningServerProxy& proxy, const std::filesystem::path& targetPath) -> rxcpp::observable<FakeVoid> {
      X509CertificateChain chain(X509CertificatesFromPem(ReadFile(targetPath)));
      return proxy.commitCertificateReplacement(chain);
    }));
  }

};

CommandServer::CommandCertificate::CommandCertificate(CommandServer& parent) : ChildCommandOf<CommandServer>("certificate", "Administer PEP certificates", parent) {}

std::vector<std::shared_ptr<pep::commandline::Command>> CommandServer::CommandCertificate::createChildCommands() {
  return  {
    std::make_shared<CommandRequestCSR>(*this),
    std::make_shared<CommandReplace>(*this),
    std::make_shared<CommandCommit>(*this),
  };
}
