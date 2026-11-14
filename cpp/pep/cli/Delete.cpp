#include <pep/cli/Commands.hpp>
#include <pep/cli/SingleCellCommand.hpp>
#include <pep/core-client/CoreClient.hpp>
#include <pep/async/FakeVoid.hpp>
#include <pep/cli/CliApplication.hpp>
#include <pep/cli/Command.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>

#include <rxcpp/rx-lite.hpp>

#include <iostream>
#include <memory>

using namespace pep::cli;

namespace {

class CommandDelete : public SingleCellModificationCommand {
public:
  explicit CommandDelete(CliApplication& parent)
    : SingleCellModificationCommand("delete", "Delete a file", parent) {
  }

protected:
  rxcpp::observable<pep::FakeVoid> performModification(std::shared_ptr<pep::CoreClient> client, const pep::StoreData2Opts& opts, std::shared_ptr<pep::PolymorphicPseudonym> pp, const std::string& column) override {
    return WriteJson(std::cout, client->deleteData2(*pp, column, opts));
  }
};

}

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandDelete(CliApplication& parent) {
  return std::make_shared<CommandDelete>(parent);
}
