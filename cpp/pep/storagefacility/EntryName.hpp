#pragma once

#include <pep/rsk-pep/Pseudonyms.hpp>

namespace pep {

class EntryName {
private:
  std::string mParticipant; // text representation of the local SF pseudonym
  std::string mColumn;

  EntryName(std::string participant, bool validateParticipant, std::string column);
  std::tuple<const std::string&, const std::string&> memberValues() const { return std::tie(this->participant(), this->column()); }

public:
  static constexpr char DELIMITER = '/'; // Don't change: we need this to (also) serve as a path delimiter on the file system and S3 page store

  EntryName(const std::string& participant, const std::string& column) : EntryName(participant, true, column) {}
  EntryName(const LocalPseudonym& pseudonym, const std::string& column) : EntryName(pseudonym.text(), false, column) {}

  const std::string& participant() const noexcept { return mParticipant; }
  const std::string& column() const noexcept { return mColumn; }

  LocalPseudonym pseudonym() const { return LocalPseudonym::FromText(this->participant()); }
  std::string string() const { return this->participant() + DELIMITER + this->column(); }

  bool operator==(const EntryName& rhs) const { return this->memberValues() == rhs.memberValues(); }
  bool operator<(const EntryName& rhs) const { return this->memberValues() < rhs.memberValues(); }

  static EntryName Parse(const std::string& sfId);
};

}
