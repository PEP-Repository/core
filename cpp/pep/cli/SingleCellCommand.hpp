#pragma once

#include <pep/cli/Command.hpp>
#include <pep/core-client/CoreClient_fwd.hpp>
#include <pep/accessmanager/AccessManagerMessages.hpp>

namespace pep::cli {

class SingleCellCommand : public ChildCommandOf<CliApplication> {
protected:
  explicit inline SingleCellCommand(const std::string& name, const std::string& description, CliApplication& parent)
    : ChildCommandOf<CliApplication>(name, description, parent) {
  }

  std::optional<std::string> getAdditionalDescription() const override;
  pep::commandline::Parameters getSupportedParameters() const override;
  void finalizeParameters() override;
  int execute() override;

  virtual std::vector<std::string> ticketAccessModes() const = 0;
  virtual rxcpp::observable<pep::FakeVoid> processCell(std::shared_ptr<pep::CoreClient> client, const pep::IndexedTicket2& ticket, const pep::PolymorphicPseudonym& pp, const std::string& column) = 0;

  static rxcpp::observable<pep::FakeVoid> WriteJson(std::ostream& destination, rxcpp::observable<pep::HistoryResult> results);
};

class SingleCellModificationCommand : public SingleCellCommand {
protected:
  explicit inline SingleCellModificationCommand(const std::string& name, const std::string& description, CliApplication& parent)
    : SingleCellCommand(name, description, parent) {
  }

  std::vector<std::string> ticketAccessModes() const override;
  rxcpp::observable<pep::FakeVoid> processCell(std::shared_ptr<pep::CoreClient> client, const pep::IndexedTicket2& ticket, const pep::PolymorphicPseudonym& pp, const std::string& column) override;

  virtual rxcpp::observable<pep::FakeVoid> performModification(std::shared_ptr<pep::CoreClient> client, const pep::storeData2Opts& opts, std::shared_ptr<pep::PolymorphicPseudonym> pp, const std::string& column) = 0;
};

}
