#pragma once

#include <pep/async/EmscriptenValPtr.hpp>
#include <pep/core-client/CoreClient.hpp>
#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/morphing/Metadata.hpp>
#include <pep/weblib/EmscriptenBuffer.hpp>

#include <rxcpp/rx-lite.hpp>

#include <compare>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declaration
namespace emscripten { class val; }

// Structures used in weblib API

namespace pep::weblib {

struct ListQuery {
  std::optional<std::vector<std::string>> subjectGroups;
  std::optional<std::vector<std::string>> subjectPolymorphicPseudonyms;
  std::optional<std::vector<std::string>> columnGroups;
  std::optional<std::vector<std::string>> columns;

  [[nodiscard]] auto operator<=>(const ListQuery&) const = default;
};

struct ColumnGroup {
  std::string name;
  std::vector<std::string> columns;

  [[nodiscard]] auto operator<=>(const ColumnGroup&) const = default;
};

struct EnrolledUser {
  std::string userGroup;
  std::string user;

  [[nodiscard]] auto operator<=>(const EnrolledUser&) const = default;
};

struct SubjectGroup {
  std::string name;

  [[nodiscard]] auto operator<=>(const SubjectGroup&) const = default;
};

struct CellEntry {
  std::shared_ptr<EnumerateResult> inner;
  std::shared_ptr<SignedTicket2> ticket;

  [[nodiscard]] std::string id() const;
  [[nodiscard]] std::string subjectLocalPseudonym() const;
  [[nodiscard]] const std::string& column() const;
  [[nodiscard]] std::uint64_t fileSize() const;
  [[nodiscard]] std::unordered_map<std::string, std::optional<emscripten::val>> partialMetadataView() const;
};

struct CellData {
  const CellEntry* entry;
  EmscriptenValPtr contentReadableStream;

  CellData(const CellEntry* entry, emscripten::val contentReadableStream);

  [[nodiscard]] emscripten::val content() const;
};

}
