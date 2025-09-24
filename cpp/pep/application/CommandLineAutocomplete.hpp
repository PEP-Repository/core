#pragma once

#include <pep/application/CommandLineCommand.hpp>
#include <pep/application/CommandLineParameter.hpp>
#include <pep/application/CommandLineSwitchAnnouncement.hpp>
#include <pep/application/CommandLineValueSpecification.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/format.hpp>

#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace pep::commandline {

namespace detail {
template <typename Range>
static void printRange(std::ostream& out, const Range& range, char separator) {
  bool separate = false;
  for (const auto& v : range) {
    if (std::exchange(separate, true)) {
      out << separator;
    }
    out << v;
  }
}
}

// See also /autocomplete/README.md for more info
class Autocomplete final {
  struct CompletionValue final {
    std::vector<std::string> valueAliases; // Non-empty
    std::string displayValue; // May be empty
    std::string description; // May be empty

    friend std::ostream& operator<<(std::ostream& out, const CompletionValue& v) {
      detail::printRange(out, v.valueAliases, valueSep);
      return out << colSep << v.displayValue << colSep << v.description;
    }

  private:
    constexpr static char colSep{ '\x1e' }, valueSep{ '\x1f' };
  };

  struct CompletionEntry final {
    std::string completionType; // e.g. parameter name/value / subcommand, may be parsed by shell script to put under known category
    std::string completionKey; // e.g. subcommands / output-file, for the user ot see. May be empty, default completionType
    std::vector<CompletionValue> values; // Values to complete. May be empty
    std::string valueType; // Completion values to be added by the shell, e.g. file/directory. May be empty

    friend std::ostream& operator<<(std::ostream& out, const CompletionEntry& e) {
      // Prefix all rows with 'suggest' in case we want to add other types later
      out << "suggest" << colSep << e.completionType << colSep << e.completionKey << colSep;
      detail::printRange(out, e.values, valueSep);
      return out << colSep << e.valueType;
    }

  private:
    // Fields are separated by ASCII separators (see https://en.wikipedia.org/wiki/C0_and_C1_control_codes#Basic_ASCII_control_codes )
    //  such that escaping is not necessary (assuming these control characters do not occur in the values)
    constexpr static char colSep{ '\x1c' }, valueSep{ '\x1d' };
  };

  std::vector<CompletionEntry> mEntries;

  std::string formatType(ArgValueType type) {
    switch (type) {
    case ArgValueType::String: return "";
    case ArgValueType::File: return "file";
    case ArgValueType::Directory: return "directory";
    }
    assert(false && "Unknown ValueType");
    return "";
  }

public:
  /*!
   * \brief Insert completion for '--'
   */
  void stopProcessingMarker() {
    mEntries.push_back({ "end subcommand", "", {{{SwitchAnnouncement::STOP_PROCESSING}, "", "End subcommand arguments"}}, {} });
  }

  /*!
   * \brief Insert completions for child commands
   * \tparam Range Range of `Command` pointers
   */
  template <typename Range>
  void childCommands(const Range& commands) {
    std::vector<CompletionValue> values;
    for (const auto& commandPtr : commands) {
      const Command& command = *commandPtr;
      if(command.isUndocumented()) {
        continue;
      }
      values.push_back({ {command.getName()}, "", command.getDescription() });
    }
    mEntries.push_back({ "subcommands", "", values, {} });
  }

  /*!
   * \brief Insert completions for parameter names, or values for positional parameters
   * \tparam Range Range of `Parameter` pointers
   */
  template <typename Range>
  void parameters(const Range& params) {
    std::vector<CompletionValue> switches;
    for (const auto& paramPtr : params) {
      const Parameter& param = *paramPtr;
      if (auto canonicalSwitch = param.getCanonicalAnnouncement()) {
        const std::string canonicalSwitchStr = canonicalSwitch->string();
        // Canonical switch announcement + aliases
        std::vector<std::string> switchAliases{ canonicalSwitchStr };
        for (const SwitchAnnouncement& sw : param.getAnnouncements()) {
          const std::string switchStr = sw.string();
          if (switchStr != canonicalSwitchStr) {
            switchAliases.push_back(switchStr);
          }
        }

        std::string displayValue = boost::algorithm::join(switchAliases, "/");
        if (const auto valueSpec = param.getValueSpecification()) {
          displayValue += valueSpec->isRequired() ? " <...>" : " [...]";
        }

        switches.push_back({ switchAliases, displayValue, param.getDescription().value_or("") });
      }
      else { // Positional
        parameterValues(param);
      }
    }
    mEntries.push_back({ "parameters", "", switches, {} });
  }

  /*!
   * \brief Insert completions for values of this parameter
   */
  void parameterValues(const Parameter& param) {
    if (const auto valueSpec = param.getValueSpecification()) {
      std::vector<CompletionValue> values;
      const auto defaultVal = valueSpec->getDefault();
      if (defaultVal) {
        // Mark default value
        const auto& [defaultStr, defaultDescription] = *defaultVal;
        values.push_back({ {defaultStr},
          (boost::format("%s (%s)") % defaultStr % defaultDescription.value_or("default")).str(),
          defaultDescription.value_or("Default") });
      }

      for (std::string suggestion : valueSpec->getSuggested()) {
        if (!defaultVal || suggestion != defaultVal->first) {
          values.push_back({ {std::move(suggestion)}, "", "" });
        }
      }

      std::string key = param.getName();
      if (auto description = param.getDescription()) {
        key = (boost::format("%s: %s") % param.getName() % *description).str();
      }
      mEntries.push_back({ "values", std::move(key), std::move(values), formatType(valueSpec->getType()) });
    }
  }

  /*!
   * \brief Write out accumulated completions in machine-readable format
   */
  void write(std::ostream& out) const {
    for (const CompletionEntry& e : mEntries) {
      out << e << '\n'; // Also end with newline to ease parsing in scripts
    }
  }
};

}
