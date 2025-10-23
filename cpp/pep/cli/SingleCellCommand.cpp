#include <pep/cli/SingleCellCommand.hpp>
#include <pep/cli/TicketFile.hpp>

#include <pep/async/RxBeforeTermination.hpp>
#include <pep/async/RxInstead.hpp>
#include <pep/core-client/CoreClient.hpp>

#include <boost/algorithm/hex.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-tap.hpp>

using namespace pep::cli;

namespace {
  std::string GetRequiredRowSpecMessage() {
    return "Please specify either --participant or --short-pseudonym but not both";
  }
}

std::optional<std::string> SingleCellCommand::getAdditionalDescription() const {
  return GetRequiredRowSpecMessage();
}

pep::commandline::Parameters SingleCellCommand::getSupportedParameters() const {
  return ChildCommandOf<CliApplication>::getSupportedParameters()
    + pep::commandline::Parameter("column", "Column name").shorthand('c').value(pep::commandline::Value<std::string>().required())
    + pep::commandline::Parameter("participant", "Polymorphic pseudonym or identifier of participant").shorthand('p').value(pep::commandline::Value<std::string>())
    + pep::commandline::Parameter("short-pseudonym", "Short pseudonym of participant").alias("sp").value(pep::commandline::Value<std::string>())
    + pep::cli::TicketFile::GetParameters(true);
}

void SingleCellCommand::finalizeParameters() {
  ChildCommandOf<CliApplication>::finalizeParameters();

  const auto& parameterValues = this->getParameterValues();
  if (parameterValues.has("participant") == parameterValues.has("short-pseudonym")) {
    throw std::runtime_error(GetRequiredRowSpecMessage());
  }
}

int SingleCellCommand::execute() {
  return this->executeEventLoopFor([this](std::shared_ptr<pep::CoreClient> client) {
    const auto& parameterValues = this->getParameterValues();
    auto column = parameterValues.get<std::string>("column");
    rxcpp::observable<pep::PolymorphicPseudonym> pp_obs;
    if (parameterValues.has("participant")) {
      pp_obs = client->parsePPorIdentity(parameterValues.get<std::string>("participant"));
    }
    else {
      assert(parameterValues.has("short-pseudonym"));
      pp_obs = client->findPPforShortPseudonym(parameterValues.get<std::string>("short-pseudonym"));
    }

    return pp_obs
      .flat_map([this, client, column](pep::PolymorphicPseudonym pp) {
      const auto& parameterValues = this->getParameterValues();

      pep::requestTicket2Opts tOpts;
      tOpts.pps = { pp };
      tOpts.columns = { column };
      tOpts.modes = this->ticketAccessModes();
      return pep::cli::TicketFile::GetTicket(*client, parameterValues, tOpts)
        .flat_map([this, client, pp, column](const pep::IndexedTicket2& ticket) {return this->processCell(client, ticket, pp, column); });
        });
    });
}

rxcpp::observable<pep::FakeVoid> SingleCellCommand::WriteJson(std::ostream& destination, rxcpp::observable<pep::HistoryResult> results) {
  destination << "[";
  auto reported = pep::MakeSharedCopy(false);
  return results.tap([&destination, reported](const pep::HistoryResult& entry) {
    if (*reported) {
      destination << ',';
    }
    else {
      *reported = true;
    }

    destination << '\n'
      << "\t{\n"
      << "\t\t\"timestamp\": " << entry.mTimestamp.getTime() << ",\n"
      << "\t\t\"pp\": \"" << entry.mLocalPseudonyms->mPolymorphic.text() << "\",\n"
      << "\t\t\"column\": \"" << entry.mColumn << "\",\n"
      << "\t\t\"id\": ";
    if (entry.mId.has_value()) {
      destination << '"' << boost::algorithm::hex(*entry.mId) << '"';
    }
    else {
      destination << "null";
    }
    destination << '\n'
      << "\t}";
    })
    .op(pep::RxBeforeTermination([&destination, reported](std::optional<std::exception_ptr>) {
      if (*reported) {
        destination << '\n';
      }
      destination << ']' << std::endl;
      }))
    .op(pep::RxInstead(pep::FakeVoid()));
}

std::vector<std::string> SingleCellModificationCommand::ticketAccessModes() const {
  return { "write" };
}

rxcpp::observable<pep::FakeVoid> SingleCellModificationCommand::processCell(std::shared_ptr<pep::CoreClient> client, const pep::IndexedTicket2& ticket, const pep::PolymorphicPseudonym& pp, const std::string& column) {
  pep::storeData2Opts opts;
  opts.ticket = pep::MakeSharedCopy(ticket);
  opts.forceTicket = true;
  return this->performModification(client, opts, pep::MakeSharedCopy(pp), column);
}
