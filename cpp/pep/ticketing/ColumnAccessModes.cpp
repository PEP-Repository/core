#include <pep/ticketing/ColumnAccessModes.hpp>

#include <pep/serialization/Error.hpp>
#include <pep/utils/Log.hpp>

#include <boost/algorithm/string/join.hpp>

namespace pep {

std::vector<std::string> ToIndividualStrings(const ColumnAccessModes modes) {
  static_assert(
      ColumnAccessModes::All == (ColumnAccessModes::Read | ColumnAccessModes::ReadMeta | ColumnAccessModes::Write | ColumnAccessModes::WriteMeta | ColumnAccessModes::_bogus_AmaTranscryptionOnly),
      "every mode must be handled below");
  std::vector<std::string> strs{};
  if (HasFlags(modes, ColumnAccessModes::Read)) { strs.emplace_back("read"); }
  if (HasFlags(modes, ColumnAccessModes::ReadMeta)) { strs.emplace_back("read-meta"); }
  if (HasFlags(modes, ColumnAccessModes::Write)) { strs.emplace_back("write"); }
  if (HasFlags(modes, ColumnAccessModes::WriteMeta)) { strs.emplace_back("write-meta"); }
  if (HasFlags(modes, ColumnAccessModes::_bogus_AmaTranscryptionOnly)) { strs.emplace_back("enumerate"); }
  return strs;
}

std::string ToSingleString(const ColumnAccessModes modes, const std::string_view separator) {
  if (modes == ColumnAccessModes::None) { return "none"; }
  return boost::join(ToIndividualStrings(modes), separator);
}

ColumnAccessModes ParseMode(const std::string_view name) {
  if (name == "read") { return ColumnAccessModes::Read; }
  if (name == "read-meta") { return ColumnAccessModes::ReadMeta; }
  if (name == "write") { return ColumnAccessModes::Write; }
  if (name == "write-meta") { return ColumnAccessModes::WriteMeta; }
  if (name == "enumerate") { return ColumnAccessModes::_bogus_AmaTranscryptionOnly; }
  throw Error("Unknown ticket mode " + Logging::Escape(std::string(name)));
}

ColumnAccessModes ParseModes(const std::span<const std::string> names) {
  auto result = ColumnAccessModes::None;
  for (const auto& name : names) {
    result |= ParseMode(name);
  }
  return result;
}

} // namespace pep
