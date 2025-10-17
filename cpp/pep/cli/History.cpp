#include <pep/cli/Commands.hpp>
#include <pep/cli/SingleCellCommand.hpp>
#include <pep/utils/Defer.hpp>
#include <pep/async/RxConcatenateVectors.hpp>
#include <pep/core-client/CoreClient.hpp>

#include <rxcpp/operators/rx-concat_map.hpp>

using namespace pep::cli;

namespace {

class CommandHistory : public SingleCellCommand {
public:
  explicit CommandHistory(CliApplication& parent)
    : SingleCellCommand("history", "Retrieve content history for a cell", parent) {
  }

protected:
  std::vector<std::string> ticketAccessModes() const override { return { "read-meta" }; }

  rxcpp::observable<pep::FakeVoid> processCell(std::shared_ptr<pep::CoreClient> client, const pep::IndexedTicket2& ticket, const pep::PolymorphicPseudonym& pp, const std::string& column) override {
    auto entries = client->getHistory2(*ticket.getTicket(), std::vector<pep::PolymorphicPseudonym>({ pp }), std::vector<std::string>({ column }))
      .op(pep::RxConcatenateVectors())
      .concat_map([](std::shared_ptr<std::vector<pep::HistoryResult>> results) {
      struct CompareHistoryResults
      {
        inline bool operator()(const pep::HistoryResult& lhs, const pep::HistoryResult& rhs) const {
          return lhs.mTimestamp.getTime() < rhs.mTimestamp.getTime();
        }
      };
      std::sort(results->begin(), results->end(), CompareHistoryResults());
      return rxcpp::observable<>::iterate(std::move(*results));
        });
    return WriteJson(std::cout, entries);
  }
};

}

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandHistory(CliApplication& parent) {
  return std::make_shared<CommandHistory>(parent);
}
