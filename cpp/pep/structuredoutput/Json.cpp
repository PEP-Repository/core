#include <pep/structuredoutput/Json.hpp>

#include <pep/utils/ChronoUtil.hpp>
#include <sstream>
#include <string>
#include <string_view>

namespace pep::structuredOutput::json {
namespace {

/// Calls \p perItem on each element of \p items in order and alternates it with calls to \p betweenItems.
/// @example `interweave({1, 2, 3}, p, b)` is equivalent to `p(1); b(); p(2); b(); p(3);`
template <typename T, typename UnaryFunction, typename Procedure>
void interweave(const std::vector<T>& items, const UnaryFunction& perItem, const Procedure& betweenItems) {
  if (items.empty()) {
    return;
  }

  perItem(items.front());
  for (auto i = items.begin() + 1; i != items.end(); ++i) {
    betweenItems();
    perItem(*i);
  }
}

std::ostream& appendUnicodeEscaped(std::ostream& stream, const char c) {
  return stream << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
}

std::ostream& appendEscaped(std::ostream& stream, const char c) {
  // Adapted from https://stackoverflow.com/a/33799784
  switch (c) {
  case '"':
    return stream << "\\\"";
  case '\\':
    return stream << "\\\\";
  case '\b':
    return stream << "\\b";
  case '\f':
    return stream << "\\f";
  case '\n':
    return stream << "\\n";
  case '\r':
    return stream << "\\r";
  case '\t':
    return stream << "\\t";
  default:
    if ('\x00' <= c && c <= '\x1f') {
      return appendUnicodeEscaped(stream, c);
    }
    else {
      return stream << c;
    }
  }
}

std::ostream& appendEscaped(std::ostream& stream, std::string_view str) {
  for (const auto c : str) {
    appendEscaped(stream, c);
  }
  return stream;
}

std::ostream& appendLiteral(std::ostream& stream, std::string_view str) {
  stream << '"';
  appendEscaped(stream, str);
  stream << '"';
  return stream;
}

/// Wrapper to allow overloading operator<< to inline calls to appendLiteral.
struct Literal final {
  std::string_view str;
};

std::ostream& operator<<(std::ostream& lhs, const Literal& rhs) {
  return appendLiteral(lhs, rhs.str);
}

std::ostream& append(std::ostream& stream, const pep::UserGroup& group, int indentLevel) {
  const auto ind = [&indentLevel]() { return indentations(indentLevel); };
  const auto maxAuth = group.mMaxAuthValidity;

  stream << Literal{group.mName} << ": ";
  if (maxAuth) {
    stream << "{\n";
    ++indentLevel;
    stream << ind() << Literal{stringConstants::maxAuthValidityKey} << ": " << Literal{pep::chrono::ToString(*maxAuth)}
           << "\n";
    --indentLevel;
    stream << ind() << "}";
  }
  else {
    stream << "{}";
  }

  return stream;
}

std::ostream& append(std::ostream& stream, const pep::QRUser& user, DisplayConfig config) {
  const auto ind = [&config]() { return indentations(config.indent); };
  const auto printUserGroups = config.flags & DisplayConfig::Flags::printUserGroups;

  stream << "{\n";
  ++config.indent;
  if (user.mDisplayId) {
    stream << ind() << Literal{stringConstants::displayIdKey} << ": " << Literal{*user.mDisplayId} << ",\n";
  }
  if (user.mPrimaryId) {
    stream << ind() << Literal{stringConstants::primaryIdKey} << ": " << Literal{*user.mPrimaryId} << ",\n";
  }
  stream << ind() << Literal{stringConstants::otherIdentifiersKey} << ": [";
  if (!user.mOtherUids.empty()) {
    stream << "\n";
    ++config.indent;
    interweave(user.mOtherUids, [&](const std::string& i) { stream << ind() << Literal{i}; }, [&] { stream << ",\n"; });
    --config.indent;
    stream << "\n" << ind();
  }
  stream << "],\n";
  if(printUserGroups) {
    stream << ind() << Literal{stringConstants::groupsKey} << ": [";
    if (!user.mGroups.empty()) {
      stream << "\n";
      ++config.indent;
      interweave(user.mGroups, [&](const std::string& g) { stream << ind() << Literal{g}; }, [&] { stream << ",\n"; });
      --config.indent;
      stream << "\n" << ind();
    }
    stream << "]\n";
  }
  --config.indent;
  stream << ind() << "}";

  return stream;
}

std::ostream& append(std::ostream& stream, const std::vector<pep::UserGroup>& groups, DisplayConfig config) {
  const auto ind = [&config]() { return indentations(config.indent); };
  const auto includeHeader = config.flags & DisplayConfig::Flags::printHeaders;

  if (includeHeader) {
    stream << Literal{stringConstants::userGroups.descriptive} << ": ";
  }
  stream << "{\n";
  ++config.indent;
  interweave(
      groups,
      [&](const pep::UserGroup& g) {
        stream << ind();
        append(stream, g, config.indent);
      },
      [&] { stream << ",\n"; });
  --config.indent;
  stream << "\n" << ind() << "}";

  return stream;
}

std::ostream& append(std::ostream& stream, const std::vector<pep::QRUser>& users, DisplayConfig config) {
  const auto ind = [&config]() { return indentations(config.indent); };
  const auto includeHeader = config.flags & DisplayConfig::Flags::printHeaders;

  if (includeHeader) {
    stream << Literal{stringConstants::users.descriptive} << ": ";
  }
  stream << "[\n";
  ++config.indent;
  interweave(
      users,
      [&](const pep::QRUser& u) {
        stream << ind();
        append(stream, u, config);
      },
      [&] { stream << ",\n"; });
  --config.indent;
  stream << "\n" << ind() << "]";

  return stream;
}

} // namespace

std::ostream& append(std::ostream& stream, const pep::UserQueryResponse& response, DisplayConfig config) {
  const auto ind = [&config]() { return indentations(config.indent); };
  const auto printHeaders = config.flags & DisplayConfig::Flags::printHeaders;
  const auto printGroups = config.flags & DisplayConfig::Flags::printGroups;
  const auto printUsers = config.flags & DisplayConfig::Flags::printUsers;

  if (printHeaders) {
    stream << ind() << "{\n";
    ++config.indent;
  }
  if (printGroups) {
    stream << ind();
    append(stream, response.mUserGroups, config);
  }
  if (printGroups && printUsers) {
    stream << ",\n";
  }
  if (printUsers) {
    stream << ind();
    append(stream, response.mUsers, config);
  }
  if (printHeaders) {
    --config.indent;
    stream << "\n" << ind() << "}";
  }

  return stream;
}

} // namespace pep::structuredOutput::json
