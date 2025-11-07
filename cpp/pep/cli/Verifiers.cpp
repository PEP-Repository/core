#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/core-client/CoreClient.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/rsk/RskSerializers.hpp>

#include <rxcpp/operators/rx-map.hpp>

using namespace pep::cli;

namespace {

class CommandVerifiers : public ChildCommandOf<CliApplication> {
public:
  explicit CommandVerifiers(CliApplication& parent)
    : ChildCommandOf<CliApplication>("verifiers", "Retrieves zero-knowledge proof verifiers", parent) {
  }

protected:
  int execute() override {
    return this->executeEventLoopFor(
      false // ensureEnrolled
      , [](std::shared_ptr<pep::CoreClient> client) {
      return client->getAccessManagerProxy()->requestVerifiers().map([](
            pep::VerifiersResponse resp) {
          std::cout
            << pep::Serialization::ToJsonString(resp)
            << std::endl;
          return pep::FakeVoid();
        });
      });
  }
};

}

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandVerifiers(CliApplication& parent) {
  return std::make_shared<CommandVerifiers>(parent);
}
