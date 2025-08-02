#pragma once

#include <pep/core-client/CoreClient_fwd.hpp>
#include <pep/application/CommandLineParameter.hpp>
#include <pep/accessmanager/AccessManagerMessages.hpp>

#include <rxcpp/rx-lite.hpp>

namespace pep {
namespace cli {

struct TicketFile
{
  static commandline::Parameters GetParameters(bool commandProvidesQuery);

  static rxcpp::observable<IndexedTicket2> GetTicket(CoreClient& client, const commandline::NamedValues& parameterValues, const std::optional<requestTicket2Opts>& opts);
};

}
}
