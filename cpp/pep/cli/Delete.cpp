#include <pep/cli/SingleCellCommand.hpp>
#include <pep/async/RxUtils.hpp>

class CommandDelete : public SingleCellModificationCommand {
public:
  explicit CommandDelete(CliApplication& parent)
    : SingleCellModificationCommand("delete", "Delete a file", parent) {
  }

protected:
  rxcpp::observable<pep::FakeVoid> performModification(std::shared_ptr<pep::CoreClient> client, const pep::storeData2Opts& opts, std::shared_ptr<pep::PolymorphicPseudonym> pp, const std::string& column) override {
    return WriteJson(std::cout, client->deleteData2(*pp, column, opts));
  }
};

std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandDelete(CliApplication& parent) {
  return std::make_shared<CommandDelete>(parent);
}
