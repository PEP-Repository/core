#include <pep/cli/structuredoutput/TableFromDownloadDirectory.hpp>

#include <algorithm>
#include <boost/url.hpp>
#include <boost/url/scheme.hpp>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <pep/structuredoutput/IndexedStringPool.hpp>
#include <pep/utils/File.hpp>
#include <pep/utils/VariantUtils.hpp>
#include <vector>

using namespace pep::cli;

namespace pep::structuredOutput {
namespace {

using Config = TableFromDownloadDirectoryConfig;
using FileReadFunction = std::function<std::string(std::filesystem::path)>;

struct TableTriplet final {
  IndexedStringPool<ParticipantIdentifier>::Ptr participant;
  IndexedStringPool<std::string>::Ptr column;
  std::string value;
};

struct TableTripletsAndPools final {
  std::vector<TableTriplet> triplets;
  IndexedStringPool<ParticipantIdentifier> participants;
  IndexedStringPool<std::string> columns;
};

TableTripletsAndPools triplets(
    const std::vector<RecordDescriptor>& descs,
    std::function<std::string(const ParticipantIdentifier&)> participantProjection,
    std::function<std::string(const RecordDescriptor&)> valueProjection) {
  const auto identity = [](const auto& x) { return x; };

  TableTripletsAndPools out{
      .triplets = {},
      .participants = IndexedStringPool<ParticipantIdentifier>{participantProjection},
      .columns = IndexedStringPool<std::string>{identity}};

  out.triplets.reserve(descs.size());
  std::ranges::transform(descs, std::back_inserter(out.triplets), [&out, &valueProjection](const RecordDescriptor& d) {
    return TableTriplet{
        .participant = out.participants.map(d.getParticipant()),
        .column = out.columns.map(d.getColumn()),
        .value = valueProjection(d)};
  });

  return out;
}

std::vector<std::string> Concat(std::string lhs, const std::vector<std::string_view>& rhs) {
  std::vector<std::string> combined;
  combined.reserve(rhs.size() + 1);
  combined.emplace_back(std::move(lhs));
  for (auto r: rhs) { combined.emplace_back(r); }
  return combined;
}

Table PreAllocatedTable(std::vector<std::string> header, std::size_t size) {
  auto data = std::vector<std::string>(header.size() * size); // allocate before moving from header
  return Table::FromSeparateHeaderAndData(std::move(header), std::move(data));
}

Table TableFrom(const TableTripletsAndPools& pooled, std::string idColumnName) {
  const auto participants = pooled.participants.all();
  auto table = PreAllocatedTable(Concat(std::move(idColumnName), pooled.columns.all()), participants.size());
  auto allRecords = table.records();
  for (std::size_t i = 0; i < participants.size(); ++i) { allRecords[i][0] = participants[i]; }
  for (auto& t : pooled.triplets) { allRecords[t.participant.index()][t.column.index() + 1] = t.value; }
  return table;
}

bool IsFileLike(const std::filesystem::path& path) {
  switch (std::filesystem::status(path).type()) {
  case std::filesystem::file_type::regular:
  case std::filesystem::file_type::symlink:
    return true;
  default:
    return false;
  }
}

/// Returns true iff every entry in column \p columnNr is a file smaller then \p sizeInBytes
bool AllColumnFilesAreSmaller(const Table& table, std::size_t columnNr, std::size_t sizeInBytes) {
  return AllOfFieldsInColumn(table, columnNr, [sizeInBytes](const std::string& field) {
    if (field.empty()) return true;
    if (!IsFileLike(field)) return false;
    return std::filesystem::file_size(field) < sizeInBytes;
  });
}

/// Returns true iff every entry in column \p columnNr is a file containing only printable chars
/// @warning Avoid calling this on columns with large files
bool AllColumnFilesArePrintable(const Table& table, std::size_t columnNr, const FileReadFunction& readFile) {
  return AllOfFieldsInColumn(table, columnNr, [&readFile](const std::string& field) {
    if (field.empty()) return true;
    if (!IsFileLike(field)) return false;
    const auto content = readFile(field);
    const auto isPrintable = [](char c) { return std::isprint(c); };
    return std::ranges::all_of(content, isPrintable);
  });
}

void InlineColumn(Table& table, std::size_t columnNr, const FileReadFunction& readFile) {
  ForEachFieldInColumn(table, columnNr, [&readFile](std::string& field) {
    if (field.empty()) return;
    field = readFile(field);
  });
}

std::string Apply(const Config::PathStyle::Variant& style, const std::filesystem::path& absolutePath) {
  assert(absolutePath.is_absolute());
  using Style = Config::PathStyle;
  return std::visit(
      variant_utils::Overloaded{
          [&absolutePath](Style::Absolute) { return absolutePath.string(); },
          [&absolutePath](Style::FileUri) -> std::string {
            return boost::urls::url{}
                .set_scheme_id(boost::urls::scheme::file)
                .set_path(absolutePath.string())
                .normalize_path()
                .buffer();
          },
          [&absolutePath](const Style::RelativeTo& relTo) {
            return absolutePath.lexically_relative(relTo.base).string();
          }},
      style);
}

void ApplyStyleToColumn(Table& table, std::size_t columnNr, const Config::PathStyle::Variant& style) {
  ForEachFieldInColumn(table, columnNr, [&style](std::string& field) {
    if (field.empty()) return;
    field = Apply(style, field);
  });
}

void ApplyConfiguration(Table& table, const Config& config, const std::filesystem::path& downloadDir) {
  const auto readWithCache =
      [cache = std::unordered_map<std::filesystem::path, std::string>{}](const std::filesystem::path& p) mutable {
        auto cacheEntry = cache.find(p);
        if (cacheEntry == cache.end()) { cacheEntry = cache.insert({p, pep::ReadFile(p)}).first; }
        return cacheEntry->second;
      };

  for (std::size_t col = 1; col < table.header().size(); ++col) { // skipping the column with identifiers
    // TODO: invoke only one function to iterate over fields only once: reduces run time from O(2n) to O(n)
    if (AllColumnFilesAreSmaller(table, col, config.maxInlineSizeInBytes)
        && AllColumnFilesArePrintable(table, col, readWithCache)) {
      InlineColumn(table, col, readWithCache);
    }
    else {
      table.header()[col] += config.fileReferencePostfix;
      ApplyStyleToColumn(table, col, config.pathStyle);
    }
  }
}

} // namespace

Table TableFrom(const DownloadDirectory& dir, const TableFromDownloadDirectoryConfig& config) {
  const auto records = dir.list();
  const auto pooled = triplets(
      records,
      config.idText,
      [&dir](const RecordDescriptor& record) {
        const auto filename = dir.getRecordFileName(record);
        return (filename.has_value() && exists(*filename)) ? filename->string() : "";
      });
  auto table = TableFrom(pooled, config.participantIdentifierColumnName);
  ApplyConfiguration(table, config, dir.getPath());
  return table;
}

} // namespace pep::structuredOutput
