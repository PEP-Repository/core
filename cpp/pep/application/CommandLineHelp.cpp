#include <pep/application/CommandLineHelp.hpp>

#include <iomanip>
#include <vector>

namespace pep {
namespace commandline {

namespace {

// Settable properties
const std::streamsize CONSOLE_WIDTH = 80; // Optimize for this width
const std::streamsize INDENT_WIDTH = 2;
const std::streamsize SUPPLEMENT_INDENT_WIDTH = INDENT_WIDTH * 3;

// Calculated from settable properties
const std::streamsize COLUMN_WIDTH = (CONSOLE_WIDTH - 1 - INDENT_WIDTH) / 2;
const std::string INDENT(INDENT_WIDTH, ' ');
const std::string SUPPLEMENT_INDENT(SUPPLEMENT_INDENT_WIDTH, ' ');

class HelpItemColumn {
private:
  std::string mText;

public:
  explicit HelpItemColumn(const std::string& text) : mText(text) {}
  void streamTo(std::ostream& destination) const { destination << std::setw(COLUMN_WIDTH) << std::left << mText; }
};

std::ostream& operator <<(std::ostream& lhs, const HelpItemColumn& column) {
  column.streamTo(lhs);
  return lhs;
}

}

void WriteHelpItem(std::ostream& destination, const std::string& entry, const std::string& description) {
  destination << INDENT << HelpItemColumn(entry);
  auto newline = entry.length() >= COLUMN_WIDTH;

  auto descriptionLength = description.length();
  size_t i = 0;
  do {
    // Start on a new line if the previous one extended into the description column
    if (newline) {
      destination << '\n' << INDENT << HelpItemColumn("");
    }

    // Get a single line of output
    auto line = description.substr(i, COLUMN_WIDTH);
    auto space = false;
    if (description.length() > i + COLUMN_WIDTH) { // We have more to output after this line
      // TODO: split on any char that isspace(c) instead of only ASCII space
      space = description[i + COLUMN_WIDTH] == ' '; // No further processing if the current line ended on a word boundary
      if (!space) {
        // Try to end the current line on a word boundary
        auto crop = line.find_last_of(' ');
        if (crop != std::string::npos) {
          line = line.substr(0, crop);
          space = true;
        }
        // else there's no space in the line: we'll just cut on COLUMN_WIDTH
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
  destination << SUPPLEMENT_INDENT << text << '\n';
}

}
}
