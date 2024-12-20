#pragma once

#include <pep/core-client/CoreClient.hpp>
#include <pep/application/CommandLineParameter.hpp>

namespace pep {
namespace cli {

struct TicketFile
{
  static commandline::Parameters GetParameters(bool commandProvidesQuery);

  static rxcpp::observable<IndexedTicket2> GetTicket(CoreClient& client, const commandline::NamedValues& parameterValues, const std::optional<requestTicket2Opts>& opts);
};

}
}
