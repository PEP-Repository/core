#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/cli/ProxyTraits.hpp>
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
    auto proxies = ProxyTraits::All();
    std::vector<std::string> ids;
    ids.reserve(proxies.size());
    std::transform(proxies.begin(), proxies.end(), std::back_inserter(ids), [](const ProxyTraits& traits) {return traits.server().commandLineId(); });
    std::sort(ids.begin(), ids.end());

    return ChildCommandOf<CliApplication>::getSupportedParameters()
      + pep::commandline::Parameter("server", "Restrict to specified server(s)").value(pep::commandline::Value<std::string>().positional().multiple()
        .allow(ids));
  }

  int execute() override {
    auto all = ProxyTraits::All();
    std::vector<ProxyTraits> proxies;

    std::vector<std::string> allowed = this->getParameterValues().getOptionalMultiple<std::string>("server");
    if (!allowed.empty()) {
      proxies.reserve(all.size());
      std::copy_if(all.begin(), all.end(), std::back_inserter(proxies), [&allowed](const ProxyTraits& candidate) {
        auto found = std::find(allowed.begin(), allowed.end(), candidate.server().commandLineId());
        return found != allowed.end();
        });
    }
    else {
      proxies = std::move(all);
    }

    return this->executeEventLoopFor([&proxies](std::shared_ptr<pep::Client> client) {
      return rxcpp::rxs::iterate(proxies)
        .concat_map([client](const ProxyTraits& traits) {
        return traits.getProxy(*client)->requestMetrics()
          .map([caption = traits.server().description()](pep::MetricsResponse metrics) {
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
