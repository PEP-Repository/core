#pragma once

#include <pep/rsk-pep/Pseudonyms.hpp>

#include <format>
#include <string>
#include <utility>

namespace pep {

class EntryName {
private:
  std::string participant_; // text representation of the local SF pseudonym
  std::string column_;

  EntryName(std::string participant, bool validateParticipant, std::string column);

public:
  static constexpr char Delimiter = '/'; // Don't change: we need this to (also) serve as a path delimiter on the file system and S3 page store

  EntryName(std::string participant, std::string column) : EntryName(std::move(participant), true, std::move(column)) {}
  EntryName(const LocalPseudonym& pseudonym, const std::string& column) : EntryName(pseudonym.text(), false, column) {}

  const std::string& participant() const noexcept { return participant_; }
  const std::string& column() const noexcept { return column_; }

  LocalPseudonym pseudonym() const { return LocalPseudonym::FromText(this->participant()); }
  [[nodiscard]] std::string string() const;

  auto operator<=>(const EntryName&) const = default;

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
