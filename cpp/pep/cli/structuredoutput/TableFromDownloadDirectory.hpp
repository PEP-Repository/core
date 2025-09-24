#pragma once

#include <functional>
#include <pep/cli/DownloadDirectory.hpp>
#include <pep/structuredoutput/Table.hpp>
#include <variant>

namespace pep::structuredOutput {

/// Preferences for DownloadDirectory to structuredOutput::Table conversion
struct TableFromDownloadDirectoryConfig final {
  using IdTextFunction = std::function<std::string(const cli::ParticipantIdentifier&)>;

  /// psuedo-namespace to hold all path styling options
  struct PathStyle final {
    PathStyle() = delete;

    ///@example '/home/user/file.txt'
    struct Absolute final {};

    ///@example 'file://home/user/file.txt'
    struct FileUri final {};

    ///@example 'user/file.txt' when relative to '/home/'
    struct RelativeTo final {
      std::filesystem::path base;
    };

    using Variant = std::variant<Absolute, FileUri, RelativeTo>;
  };

  /// Only columns where all files are smaller or equal to this size are considered for inlining
  std::size_t maxInlineSizeInBytes = 100;

  /// How paths are presented in the resulting table
  PathStyle::Variant pathStyle;

  /// The name to use for the first column, which contains a unique identifier for each row
  std::string participantIdentifierColumnName = "id";

  /// The postfix added to the name of a column when it contains references to external files
  /// or directories, i.e., any column that is not inlined.
  std::string fileReferencePostfix = " (file ref)";

  /// How participant ids are converted to text
  IdTextFunction idText = [](const cli::ParticipantIdentifier& id) { return id.getLocalPseudonym().text(); };
};

/// @brief Converts a DownloadDirectory to a Table
/// @return A Table with the following properties:
///   - The first column contains identifiers for the participants.
///   - Every other column in the table matches a column represented in the DownloadDirectory.
///     Individual individual fields contain either an absolute path to a file in the DownloadDirectory,
///     or an empty string to indicate the absence of a file.
///   - Each row matches a participant reprented in the DownloadDirectory.
///   - Rows are sorted by parcitipant id (first column) in ascending order.
structuredOutput::Table TableFrom(const cli::DownloadDirectory&, const TableFromDownloadDirectoryConfig& = {});

} // namespace pep::structuredOutput
