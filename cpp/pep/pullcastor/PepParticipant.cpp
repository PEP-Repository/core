#include <pep/async/RxInstead.hpp>
#include <pep/async/RxIterate.hpp>
#include <pep/core-client/CoreClient.hpp>
#include <pep/pullcastor/PepParticipant.hpp>
#include <pep/pullcastor/PullCastorUtils.hpp>

#include <rxcpp/operators/rx-flat_map.hpp>

namespace pep {
namespace castor {

PepParticipant::PepParticipant(const PolymorphicPseudonym& pp)
  : pp_(pp) {
}

void PepParticipant::loadCell(std::shared_ptr<CoreClient> client, std::shared_ptr<SignedTicket2> ticket, const EnumerateAndRetrieveResult& ear) {
  assert(pp_ == ear.localPseudonyms->polymorphic_);

  auto column = ear.column;
  auto content = CellContent::Create(client, ticket, ear);
  if (!cells_.emplace(std::make_pair(column, content)).second) {
    throw std::runtime_error("Cannot store duplicate cell for column " + column);
  }
}

rxcpp::observable<std::shared_ptr<PepParticipant>> PepParticipant::LoadAll(std::shared_ptr<CoreClient> client,
  const std::vector<PolymorphicPseudonym>& participants,
  const std::vector<std::string>& participantGroups,
  const std::vector<std::string>& columns,
  const std::vector<std::string>& columnGroups) {
  RequestTicket2Opts ticketOpts;
  ticketOpts.modes = { "read" };
  ticketOpts.pps = participants;
  ticketOpts.participantGroups = participantGroups;
  ticketOpts.columns = columns;
  ticketOpts.columnGroups = columnGroups;

  return client->requestTicket2(ticketOpts)
    .flat_map([client, ticketOpts](IndexedTicket2 ticket) {
    auto signedTicket = ticket.getTicket();  // Capture before move of ticket

    EnumerateAndRetrieveData2Opts earOpts;
    earOpts.pps = ticketOpts.pps;
    earOpts.groups = ticketOpts.participantGroups;
    earOpts.columnGroups = ticketOpts.columnGroups;
    earOpts.columns = ticketOpts.columns;
    earOpts.ticket = std::make_shared<IndexedTicket2>(std::move(ticket));
    earOpts.forceTicket = true;

    auto participants = std::make_shared<std::unordered_map<uint32_t, std::shared_ptr<PepParticipant>>>();
    return client->enumerateAndRetrieveData2(earOpts)
      .map([client, ticket = std::move(signedTicket), participants](EnumerateAndRetrieveResult ear) {
      std::shared_ptr<PepParticipant> participant;
      auto position = participants->find(ear.localPseudonymsIndex);
      if (position == participants->cend()) {
        participant = PepParticipant::Create(ear.localPseudonyms->polymorphic_);
        participants->emplace(std::make_pair(ear.localPseudonymsIndex, participant));
      }
      else {
        participant = position->second;
      }

      participant->loadCell(client, ticket, ear);
      return FakeVoid();
      })
      .op(RxInstead(participants))
      .flat_map([](auto participants) {return RxIterate(*participants); })
      .map([](const auto& pair) {return pair.second; });
    });
}

std::shared_ptr<const CellContent> PepParticipant::tryGetCellContent(const std::string& column) const {
  auto found = cells_.find(column);
  if (found != cells_.cend()) {
    return found->second;
  }
  return nullptr;
}

}
}
