#include <pep/accessmanager/AccessManagerMessages.hpp>

#include <format>
#include <ranges>

namespace pep {

std::shared_ptr<SignedTicket2> IndexedTicket2::getTicket() const {
  return ticket_;
}

std::vector<std::string> IndexedTicket2::getColumnGroups() const {
  return std::views::keys(columnGroups_) | std::ranges::to<std::vector>();
}

std::vector<std::string> IndexedTicket2::getParticipantGroups() const {
  return std::views::keys(participantGroups_) | std::ranges::to<std::vector>();
}

std::vector<std::string> IndexedTicket2::getColumns() const {
  return openTicketWithoutCheckingSignature()->columns;
}

std::vector<std::string> IndexedTicket2::getModes() const {
  return openTicketWithoutCheckingSignature()->modes;
}


std::vector<PolymorphicPseudonym> IndexedTicket2::getAccessSubjects() const {
  return GetPolymorphicPseudonyms(openTicketWithoutCheckingSignature()->accessSubjects);
}

std::shared_ptr<Ticket2> IndexedTicket2::openTicketWithoutCheckingSignature() const {
  std::lock_guard<std::mutex> lock(unpackedTicketLock_);
  if (unpackedTicket_ == nullptr)
    unpackedTicket_ = std::make_shared<Ticket2>(
      ticket_->openWithoutCheckingSignature());
  return unpackedTicket_;
}

const std::unordered_map<std::string, IndexList>&
IndexedTicket2::getColumnGroupMapping() const {
  return columnGroups_;
}

const std::unordered_map<std::string, IndexList>&
IndexedTicket2::getParticipantGroupMapping() const {
  return participantGroups_;
}

std::string StructureMetadataKey::toString() const {
  return std::format("{}:{}", metadataGroup, subkey);
}

}
