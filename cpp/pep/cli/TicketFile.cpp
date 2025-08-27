#include <pep/cli/TicketFile.hpp>
#include <pep/utils/File.hpp>
#include <pep/accessmanager/AccessManagerSerializers.hpp>
#include <pep/core-client/CoreClient.hpp>

#include <rxcpp/operators/rx-tap.hpp>
#include <fstream>

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

rxcpp::observable<IndexedTicket2> TicketFile::GetTicket(CoreClient& client, const commandline::NamedValues& parameterValues, const std::optional<requestTicket2Opts>& opts) {
  auto requestOpts = opts.value_or(requestTicket2Opts());
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
    assert(!requestOpts.modes.empty());
  }

  std::optional<std::filesystem::path> file;
  if (parameterValues.has("ticket-out")) {
    assert(opts.has_value());
    file = parameterValues.get<std::filesystem::path>("ticket-out");
    // Tickets are written to file with the intent to use them for followup (e.g. "pepcli get") queries, which require the "read" privilege
    auto mode = std::find(requestOpts.modes.cbegin(), requestOpts.modes.cend(), "read");
    if (mode == requestOpts.modes.cend()) {
      requestOpts.modes.push_back("read");
    }
  }

  rxcpp::observable<IndexedTicket2> result = client.requestTicket2(requestOpts);
  if (file.has_value()) {
    result = result.tap([file](const IndexedTicket2& ticket) {
      std::ofstream tso(file->string(), std::ios_base::out | std::ios_base::binary);
      tso << pep::Serialization::ToString(ticket);
    });
  }

  return result;
}

}
}
