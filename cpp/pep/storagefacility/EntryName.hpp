#pragma once

#include <pep/rsk-pep/Pseudonyms.hpp>

namespace pep {

class EntryName {
private:
  std::string participant_; // text representation of the local SF pseudonym
  std::string column_;

  EntryName(std::string participant, bool validateParticipant, std::string column);

public:
  static constexpr char Delimiter = '/'; // Don't change: we need this to (also) serve as a path delimiter on the file system and S3 page store

  EntryName(const std::string& participant, const std::string& column) : EntryName(participant, true, column) {}
  EntryName(const LocalPseudonym& pseudonym, const std::string& column) : EntryName(pseudonym.text(), false, column) {}

  const std::string& participant() const noexcept { return participant_; }
  const std::string& column() const noexcept { return column_; }

  LocalPseudonym pseudonym() const { return LocalPseudonym::FromText(this->participant()); }
  std::string string() const { return this->participant() + Delimiter + this->column(); }

  auto operator<=>(const EntryName&) const = default;

  static EntryName Parse(const std::string& sfId);
};

}
