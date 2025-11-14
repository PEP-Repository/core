#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/client/Client.hpp>
#include <pep/application/Application.hpp>

#include <rxcpp/operators/rx-concat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>

using namespace pep::cli;

namespace {

class CommandMetrics : public ChildCommandOf<CliApplication> {
public:
  explicit CommandMetrics(CliApplication& parent)
    : ChildCommandOf<CliApplication>("metrics", "Retrieves metrics", parent) {
  }

protected:
  pep::commandline::Parameters getSupportedParameters() const override {
    auto traits = pep::ServerTraits::All();
    std::vector<std::string> ids;
    ids.reserve(traits.size());
    std::transform(traits.begin(), traits.end(), std::back_inserter(ids), [](const pep::ServerTraits& traits) {return traits.commandLineId(); });
    std::sort(ids.begin(), ids.end());

    return ChildCommandOf<CliApplication>::getSupportedParameters()
      + pep::commandline::Parameter("server", "Restrict to specified server(s)").value(pep::commandline::Value<std::string>().positional().multiple()
        .allow(ids));
  }

  int execute() override {
    return this->executeEventLoopFor([allowed = this->getParameterValues().getOptionalMultiple<std::string>("server")](std::shared_ptr<pep::Client> client) {
      pep::Client::ServerProxies proxies;

      if (allowed.empty()) {
        proxies = client->getServerProxies(true);
      }
      else {
        auto available = client->getServerProxies(false);

        for (const auto& id : allowed) {
          auto found = std::find_if(available.begin(), available.end(), [id](const auto& pair) {
            return pair.first.commandLineId() == id;
            });
          if (found == available.end()) {
            throw std::runtime_error("No endpoint configured for " + id);
          }
          proxies.emplace(*found);
        }
      }

      return rxcpp::rxs::iterate(proxies)
        .concat_map([client](const auto& pair) {
        const pep::ServerTraits& traits = pair.first;
        std::shared_ptr<const pep::ServerProxy> proxy = pair.second;

        return proxy->requestMetrics()
          .map([caption = traits.description()](pep::MetricsResponse metrics) {
          std::cout
            << "============================ " << caption << " ============================\n"
            << metrics.mMetrics
            << std::endl;
          return pep::FakeVoid();
            });
          });
    });
  }
};

}

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandMetrics(CliApplication& parent) {
  return std::make_shared<CommandMetrics>(parent);
}
