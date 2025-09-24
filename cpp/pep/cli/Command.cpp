#include <pep/client/Client.hpp>
#include <pep/cli/Command.hpp>

namespace pep::cli {

int ChildCommand::executeEventLoopForClient(bool ensureEnrolled, std::function<rxcpp::observable<pep::FakeVoid>(std::shared_ptr<pep::CoreClient> client)> callback) {
  return this->executeEventLoopFor(ensureEnrolled, [callback](std::shared_ptr<Client> client) { return callback(client); });
}

}
