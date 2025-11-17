#include <pep/auth/ServerTraits.hpp>
#include <gtest/gtest.h>

namespace {

// Not using a map<> or unordered_map<> because we'd need to use some (possibly duplicate) property as the key
template <typename T> using ServerProperty = std::pair<std::string, T>;
template <typename T> using ServerProperties = std::vector<ServerProperty<T>>;

template <typename T>
void VerifyServerPropertiesAreUnique(const std::string& description, const ServerProperties<T>& properties);

template <>
void VerifyServerPropertiesAreUnique<std::string>(const std::string& description, const ServerProperties<std::string>& properties) {
  auto end = properties.end();
  for (auto i = properties.begin(); i != end; ++i) {
    auto j = i;
    for (++j; j != end; ++j) {
      EXPECT_NE(i->second, j->second) << i->first << " and " << j->first << " have the same " << description << ": \"" << i->second << '"';
    }
  }
}

template <typename T>
void VerifyServerMethodProducesUniqueProperties(const std::vector<pep::ServerTraits>& servers, const std::string& property, T (pep::ServerTraits::*method)() const) {
  using Plain = std::remove_const_t<std::remove_reference_t<T>>;
  ServerProperties<Plain> properties;
  std::transform(servers.begin(), servers.end(), std::inserter(properties, properties.begin()), [method](const pep::ServerTraits& server) {
    return std::make_pair(server.description(), (server.*method)());
    });
  VerifyServerPropertiesAreUnique(property, std::move(properties));
}

}

TEST(ServerTraits, HaveUniqueProperties) {

  auto all = pep::ServerTraits::All();

  VerifyServerMethodProducesUniqueProperties(all, "abbreviation", &pep::ServerTraits::abbreviation);
  VerifyServerMethodProducesUniqueProperties(all, "description", &pep::ServerTraits::description);
  VerifyServerMethodProducesUniqueProperties(all, "config node", &pep::ServerTraits::configNode);
  VerifyServerMethodProducesUniqueProperties(all, "command line ID", &pep::ServerTraits::commandLineId);
  VerifyServerMethodProducesUniqueProperties(all, "TLS certificate subject", &pep::ServerTraits::tlsCertificateSubject);
}
