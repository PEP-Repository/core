#include <pep/cli/TicketFile.hpp>
#include <pep/utils/File.hpp>
#include <pep/accessmanager/AccessManagerSerializers.hpp>
#include <pep/core-client/CoreClient.hpp>
#include <pep/accessmanager/AccessManagerMessages.hpp> // for IndexedTicket2
#include <pep/application/CommandLineParameter.hpp> // for commandline::Parameters
#include <pep/application/CommandLineValue.hpp> // for commandline::NamedValues
#include <pep/serialization/Serialization.hpp>
#include <pep/ticketing/ColumnAccessModes.hpp>
#include <pep/utils/Shared.hpp>

#include <rxcpp/operators/rx-tap.hpp>
#include <rxcpp/rx-lite.hpp>
#include <fstream>
#include <cassert>
#include <filesystem>
#include <ios>
#include <optional>
#include <stdexcept>

namespace pep {
namespace cli {

commandline::Parameters TicketFile::GetParameters(bool commandProvidesQuery) {
  commandline::Parameters result;

  auto readParameterValue = commandline::Value<std::filesystem::path>();
  if (commandProvidesQuery) {
    // We can request (and hence write) a ticket for this command
    result = result + commandline::Parameter("ticket-out", "Store ticket to file for follow-up queries").shorthand('T').value(commandline::Value<std::filesystem::path>());
  }
  else {
    // The command doesn't know about the row(s) and column(s) it deals with and therefore requires an externally provided ticket
    readParameterValue = readParameterValue.required();
  }
  return result + commandline::Parameter("ticket", "Use ticket stored in this file").shorthand('t').value(readParameterValue);
}

rxcpp::observable<IndexedTicket2> TicketFile::GetTicket(CoreClient& client, const commandline::NamedValues& parameterValues, const std::optional<RequestTicket2Opts>& opts) {
  auto requestOpts = opts.value_or(RequestTicket2Opts());
  assert(requestOpts.ticket == nullptr);
  assert(!requestOpts.forceTicket);

  if (parameterValues.has("ticket")) {
    requestOpts.ticket = MakeSharedCopy(
      pep::Serialization::FromString<pep::IndexedTicket2>(
        pep::ReadFile(parameterValues.get<std::filesystem::path>("ticket"))));
    requestOpts.forceTicket = true;
  }
  else if (!opts.has_value()) {
    throw std::runtime_error("Ticket request options must be passed if an external ticket is not provided");
  }
  else {
    assert(requestOpts.modes != ColumnAccessModes::None);
  }

  std::optional<std::filesystem::path> file;
  if (parameterValues.has("ticket-out")) {
    assert(opts.has_value());
    file = parameterValues.get<std::filesystem::path>("ticket-out");
    // Tickets are written to file with the intent to use them for followup (e.g. "pepcli get") queries, which require the "read" privilege
    requestOpts.modes |= ColumnAccessModes::Read;
  }

  rxcpp::observable<IndexedTicket2> result = client.requestTicket2(requestOpts);
  if (file.has_value()) {
    result = result.tap([file](const IndexedTicket2& ticket) {
      std::ofstream tso(*file, std::ios_base::out | std::ios_base::binary);
      tso << pep::Serialization::ToString(ticket);
    });
  }

  return result;
}

}
}
