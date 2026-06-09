#include <pep/async/RxInstead.hpp>
#include <pep/async/RxIterate.hpp>
#include <pep/core-client/CoreClient.hpp>
#include <pep/pullcastor/PepParticipant.hpp>
#include <pep/pullcastor/PullCastorUtils.hpp>

#include <rxcpp/operators/rx-flat_map.hpp>

namespace pep {
namespace castor {

PepParticipant::PepParticipant(const PolymorphicPseudonym& pp)
  : mPp(pp) {
}

void PepParticipant::loadCell(std::shared_ptr<CoreClient> client, std::shared_ptr<SignedTicket2> ticket, const EnumerateAndRetrieveResult& ear) {
  assert(mPp == ear.mLocalPseudonyms->mPolymorphic);

  auto column = ear.mColumn;
  auto content = CellContent::Create(client, ticket, ear);
  if (!mCells.emplace(std::make_pair(column, content)).second) {
    throw std::runtime_error("Cannot store duplicate cell for column " + column);
  }
}

rxcpp::observable<std::shared_ptr<PepParticipant>> PepParticipant::LoadAll(std::shared_ptr<CoreClient> client,
  const std::vector<PolymorphicPseudonym>& participants,
  const std::vector<std::string>& participantGroups,
  const std::vector<std::string>& columns,
  const std::vector<std::string>& columnGroups) {
  requestTicket2Opts ticketOpts;
  ticketOpts.modes = { "read" };
  ticketOpts.pps = participants;
  ticketOpts.participantGroups = participantGroups;
  ticketOpts.columns = columns;
  ticketOpts.columnGroups = columnGroups;

  return client->requestTicket2(ticketOpts)
    .flat_map([client, ticketOpts](IndexedTicket2 ticket) {
    auto signedTicket = ticket.getTicket();  // Capture before move of ticket

    enumerateAndRetrieveData2Opts earOpts;
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
      auto position = participants->find(ear.mLocalPseudonymsIndex);
      if (position == participants->cend()) {
        participant = PepParticipant::Create(ear.mLocalPseudonyms->mPolymorphic);
        participants->emplace(std::make_pair(ear.mLocalPseudonymsIndex, participant));
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
  auto found = mCells.find(column);
  if (found != mCells.cend()) {
    return found->second;
  }
  return nullptr;
}

}
}
