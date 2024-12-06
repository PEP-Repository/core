#include <pep/cli/Command.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/rsk/RskSerializers.hpp>

#include <rxcpp/operators/rx-map.hpp>

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
      return client->getRSKVerifiers().map([](
            pep::VerifiersResponse resp) {
          std::cout
            << pep::Serialization::ToJsonString(resp)
            << std::endl;
          return pep::FakeVoid();
        });
      });
  }
};

std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandVerifiers(CliApplication& parent) {
  return std::make_shared<CommandVerifiers>(parent);
}
