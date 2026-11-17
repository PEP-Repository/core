#pragma once

#include <pep/rsk-pep/Pseudonyms.hpp>

#include <format>
#include <string>
#include <tuple>

namespace pep {

class EntryName {
private:
  std::string participant_; // text representation of the local SF pseudonym
  std::string column_;

  EntryName(std::string participant, bool validateParticipant, std::string column);
  std::tuple<const std::string&, const std::string&> memberValues() const { return std::tie(this->participant(), this->column()); }

public:
  static constexpr char Delimiter = '/'; // Don't change: we need this to (also) serve as a path delimiter on the file system and S3 page store

  EntryName(const std::string& participant, const std::string& column) : EntryName(participant, true, column) {}
  EntryName(const LocalPseudonym& pseudonym, const std::string& column) : EntryName(pseudonym.text(), false, column) {}

  const std::string& participant() const noexcept { return participant_; }
  const std::string& column() const noexcept { return column_; }

  LocalPseudonym pseudonym() const { return LocalPseudonym::FromText(this->participant()); }
  [[nodiscard]] std::string string() const;

  bool operator==(const EntryName& rhs) const { return this->memberValues() == rhs.memberValues(); }
  bool operator<(const EntryName& rhs) const { return this->memberValues() < rhs.memberValues(); }

  static EntryName Parse(const std::string& sfId);
};

}

template <>
struct std::formatter<pep::EntryName> {
  constexpr auto parse(std::format_parse_context& ctx) {
    return ctx.begin(); // We just have "{}", no format specifiers
  }

  format_context::iterator format(const pep::EntryName& name, format_context& ctx) const;
};
