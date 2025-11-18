#include <pep/auth/ServerTraits.hpp>
#include <pep/utils/MiscUtil.hpp>
#include <boost/algorithm/string/join.hpp>
#include <gtest/gtest.h>

namespace {

template <typename T> using ServerProperties = std::map<pep::ServerTraits, T>;
template <typename T> using ServerProperty = ServerProperties<T>::value_type;

template <typename T, typename Enable = void>
struct ServerPropertyValue;

template <typename T>
struct BasicServerPropertyValue {
  static bool IsEmpty(const T&) { return false; }
  static std::optional<std::string> DescribeIfDuplicate(const T& lhs, const T& rhs) {
    if (lhs == rhs) {
      return ServerPropertyValue<T>::ToString(lhs);
    }
    return std::nullopt;
  }
};

template <>
struct ServerPropertyValue<std::string> : public BasicServerPropertyValue<std::string> {
  static std::string ToString(const std::string& value) { return '"' + value + '"'; }
};

template <typename T>
struct ServerPropertyValue<T, std::enable_if_t<std::is_enum_v<T>>> : public BasicServerPropertyValue<T> {
  static std::string ToString(const T& value) { return std::to_string(pep::ToUnderlying(value)); }
};

template <typename T>
struct ServerPropertyValue<std::optional<T>> : public BasicServerPropertyValue<std::optional<T>> {
  static bool IsEmpty(const std::optional<T>& value) { return !value.has_value(); }
  static std::string ToString(const std::optional<T>& value) {
    assert(value.has_value());
    return ServerPropertyValue<T>::ToString(*value);
  }
};

template <typename T>
struct ServerPropertyValue<std::unordered_set<T>> {
  static bool IsEmpty(const std::unordered_set<T>& value) { return value.empty(); }
  static std::optional<std::string> DescribeIfDuplicate(const std::unordered_set<T>& lhs, const std::unordered_set<T>& rhs) {
    // Collect (descriptions of) duplicate entries
    std::vector<std::string> entries;
    for (const auto& i : lhs) {
      for (const auto& j : rhs) {
        if (auto duplicate = ServerPropertyValue<T>::DescribeIfDuplicate(i, j)) {
          entries.emplace_back(*duplicate);
        }
      }
    }

    if (entries.empty()) {
      return std::nullopt;
    }
    return "{ " + boost::join(entries, ", ") + " }";
  }
};

template <typename GetProperty>
void VerifyServersHaveUniqueProperties(const std::set<pep::ServerTraits>& servers, const std::string& property, const GetProperty& getProperty) {
  using T = std::invoke_result_t<const GetProperty, const pep::ServerTraits&>;

  // Aggregate the properties for all servers
  using Plain = std::remove_const_t<std::remove_reference_t<T>>;
  ServerProperties<Plain> properties;
  std::transform(servers.begin(), servers.end(), std::inserter(properties, properties.begin()), [getProperty](const pep::ServerTraits& server) {
    return std::make_pair(server, getProperty(server));
    });

  // Compare each server('s property) against each other server('s property)
  using Value = ServerPropertyValue<Plain>;
  auto end = properties.end();
  for (auto i = properties.begin(); i != end; ++i) {
    if (!Value::IsEmpty(i->second)) { // Don't compare e.g. nullopt
      auto j = i;
      for (++j; j != end; ++j) {
        if (!Value::IsEmpty(j->second)) { // Don't compare e.g. nullopt
          if (std::optional<std::string> duplicate = Value::DescribeIfDuplicate(i->second, j->second)) {
            FAIL() << i->first.description() << " and " << j->first.description() << " have duplicate \"" << property << '\" ' << *duplicate;
          }
        }
      }
    }
  }
}

template <typename T>
void VerifyServerMethodProducesUniqueProperties(const std::set<pep::ServerTraits>& servers, const std::string& property, T(pep::ServerTraits::* method)() const) {
  return VerifyServersHaveUniqueProperties(servers, property, [method](const pep::ServerTraits& server) { return (server.*method)(); });
}

template <typename T>
void VerifyServerMethodProducesUniqueProperties(const std::set<pep::ServerTraits>& servers, const std::string& property, T(pep::ServerTraits::* method)(bool) const) {
  return VerifyServersHaveUniqueProperties(servers, property, [method](const pep::ServerTraits& server) { return (server.*method)(false); });
}

}

TEST(ServerTraits, HaveUniqueProperties) {
  auto servers = pep::ServerTraits::All();
  EXPECT_EQ(servers.size(), 6U);

  VerifyServerMethodProducesUniqueProperties(servers, "abbreviation", &pep::ServerTraits::abbreviation);
  VerifyServerMethodProducesUniqueProperties(servers, "description", &pep::ServerTraits::description);
  VerifyServerMethodProducesUniqueProperties(servers, "config node", &pep::ServerTraits::configNode);
  VerifyServerMethodProducesUniqueProperties(servers, "command line ID", &pep::ServerTraits::commandLineId);

  VerifyServerMethodProducesUniqueProperties(servers, "certificate subject", &pep::ServerTraits::certificateSubject);
  VerifyServerMethodProducesUniqueProperties(servers, "certificate subjects", &pep::ServerTraits::certificateSubjects);

  VerifyServerMethodProducesUniqueProperties(servers, "user group", &pep::ServerTraits::userGroup);
  VerifyServerMethodProducesUniqueProperties(servers, "user groups", &pep::ServerTraits::userGroups);

  VerifyServerMethodProducesUniqueProperties(servers, "enrolls as party", &pep::ServerTraits::enrollsAsParty);
  VerifyServerMethodProducesUniqueProperties(servers, "enrollment subject", &pep::ServerTraits::enrollmentSubject);
}
