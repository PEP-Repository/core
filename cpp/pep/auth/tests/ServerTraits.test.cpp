#include <pep/auth/ServerTraits.hpp>
#include <gtest/gtest.h>

namespace {

// Not using a map<> or unordered_map<> because we'd need to use some (possibly duplicate) property as the key
template <typename T> using ServerProperty = std::pair<std::string, T>;
template <typename T> using ServerProperties = std::vector<ServerProperty<T>>;

template <typename T>
void VerifyServerPropertiesAreUnique(const std::string& description, ServerProperties<T> properties);

template <>
void VerifyServerPropertiesAreUnique<std::string>(const std::string& description, ServerProperties<std::string> properties) {
  auto end = properties.end();
  for (auto i = properties.begin(); i != end; ++i) {
    auto j = i;
    for (++j; j != end; ++j) {
      EXPECT_NE(i->second, j->second) << i->first << " and " << j->first << " have the same " << description << ": \"" << i->second << '"';
    }
  }
}

template <>
void VerifyServerPropertiesAreUnique<std::optional<std::string>>(const std::string& description, ServerProperties<std::optional<std::string>> properties) {
  properties.erase(std::remove_if(properties.begin(), properties.end(), [](const ServerProperty<std::optional<std::string>>& property) {
    return !property.second.has_value();
    }));

  ServerProperties<std::string> dereferenced;
  dereferenced.reserve(properties.size());
  std::transform(properties.begin(), properties.end(), std::back_inserter(dereferenced), [](const ServerProperty<std::optional<std::string>>& property) {
    return std::make_pair(property.first, *property.second);
    });
  return VerifyServerPropertiesAreUnique(description, dereferenced);
}

template <typename T>
void VerifyServerMethodProducesUniqueProperties(const std::vector<pep::ServerTraits>& servers, const std::string& property, T (pep::ServerTraits::*method)() const) {
  using Plain = std::remove_const_t<std::remove_reference_t<T>>;
  ServerProperties<Plain> properties;
  std::transform(servers.begin(), servers.end(), std::back_inserter(properties), [method](const pep::ServerTraits& server) {
    return std::make_pair(server.description(), (server.*method)());
    });
  VerifyServerPropertiesAreUnique(property, std::move(properties));
}

}

TEST(ServerTraits, HaveUniqueProperties) {
  auto servers = pep::ServerTraits::All();

  VerifyServerMethodProducesUniqueProperties(servers, "abbreviation", &pep::ServerTraits::abbreviation);
  VerifyServerMethodProducesUniqueProperties(servers, "description", &pep::ServerTraits::description);
  VerifyServerMethodProducesUniqueProperties(servers, "config node", &pep::ServerTraits::configNode);
  VerifyServerMethodProducesUniqueProperties(servers, "command line ID", &pep::ServerTraits::commandLineId);
  VerifyServerMethodProducesUniqueProperties(servers, "TLS certificate subject", &pep::ServerTraits::tlsCertificateSubject);

  VerifyServerMethodProducesUniqueProperties(servers, "user group", &pep::ServerTraits::userGroup);
  // TODO: VerifyServerMethodProducesUniqueProperties(servers, "user groups", &pep::ServerTraits::userGroups);
  // TODO: VerifyServerMethodProducesUniqueProperties(servers, "enrolls as", &pep::ServerTraits::enrollsAs);
  VerifyServerMethodProducesUniqueProperties(servers, "enrollment subject", &pep::ServerTraits::enrollmentSubject);
}
