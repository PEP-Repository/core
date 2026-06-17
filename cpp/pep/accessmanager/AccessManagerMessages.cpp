#include <pep/accessmanager/AccessManagerMessages.hpp>

#include <format>

namespace pep {

std::shared_ptr<SignedTicket2> IndexedTicket2::getTicket() const {
  return ticket_;
}

std::vector<std::string> IndexedTicket2::getColumnGroups() const {
  std::vector<std::string> ret;
  ret.reserve(columnGroups_.size());
  for (const auto& kv : columnGroups_)
    ret.push_back(kv.first);
  return ret;
}

std::vector<std::string> IndexedTicket2::getParticipantGroups() const {
  std::vector<std::string> ret;
  ret.reserve(participantGroups_.size());
  for (const auto& kv : participantGroups_)
    ret.push_back(kv.first);
  return ret;
}

std::vector<std::string> IndexedTicket2::getColumns() const {
  std::vector<std::string> ret;
  const auto& columns = openTicketWithoutCheckingSignature()->mColumns;
  ret.reserve(columns.size());
  for (const auto& column : columns)
    ret.push_back(column);
  return ret;
}

std::vector<std::string> IndexedTicket2::getModes() const {
  std::vector<std::string> ret;
  const auto& modes = openTicketWithoutCheckingSignature()->mModes;
  ret.reserve(modes.size());
  for (const auto& mode : modes)
    ret.push_back(mode);
  return ret;
}


std::vector<PolymorphicPseudonym> IndexedTicket2::getAccessSubjects() const {
  return GetPolymorphicPseudonyms(openTicketWithoutCheckingSignature()->mAccessSubjects);
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
