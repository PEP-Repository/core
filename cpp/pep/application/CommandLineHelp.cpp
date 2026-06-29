#include <pep/application/CommandLineHelp.hpp>

#include <iomanip>
#include <vector>

namespace pep {
namespace commandline {

namespace {

// Settable properties
const std::streamsize ConsoleWidth = 80; // Optimize for this width
const std::streamsize IndentWidth = 2;
const std::streamsize SupplementIndentWidth = IndentWidth * 3;

// Calculated from settable properties
const std::streamsize ColumnWidth = (ConsoleWidth - 1 - IndentWidth) / 2;
const std::string Indent(IndentWidth, ' ');
const std::string SupplementIndent(SupplementIndentWidth, ' ');

class HelpItemColumn {
private:
  std::string text_;

public:
  explicit HelpItemColumn(const std::string& text) : text_(text) {}
  void streamTo(std::ostream& destination) const { destination << std::setw(ColumnWidth) << std::left << text_; }
};

std::ostream& operator <<(std::ostream& lhs, const HelpItemColumn& column) {
  column.streamTo(lhs);
  return lhs;
}

}

void WriteHelpItem(std::ostream& destination, const std::string& entry, const std::string& description) {
  destination << Indent << HelpItemColumn(entry);
  auto newline = entry.length() >= ColumnWidth;

  auto descriptionLength = description.length();
  size_t i = 0;
  do {
    // Start on a new line if the previous one extended into the description column
    if (newline) {
      destination << '\n' << Indent << HelpItemColumn("");
    }

    // Get a single line of output
    auto line = description.substr(i, ColumnWidth);
    auto space = false;
    if (description.length() > i + ColumnWidth) { // We have more to output after this line
      // TODO: split on any char that isspace(c) instead of only ASCII space
      space = description[i + ColumnWidth] == ' '; // No further processing if the current line ended on a word boundary
      if (!space) {
        // Try to end the current line on a word boundary
        auto crop = line.find_last_of(' ');
        if (crop != std::string::npos) {
          line = line.substr(0, crop);
          space = true;
        }
        // else there's no space in the line: we'll just cut on ColumnWidth
      }
    }

    destination << HelpItemColumn(line);
    i += line.length();
    if (space) {
      ++i;
    }

    // The next iteration will need to start on a new line
    newline = true;
  } while (i < descriptionLength);
  destination << '\n';
}

void WriteHelpItemSupplement(std::ostream& destination, const std::string& text) {
  destination << SupplementIndent << text << '\n';
}

}
}
