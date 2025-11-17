#include <pep/auth/ServerTraits.hpp>
#include <pep/utils/MiscUtil.hpp>
#include <gtest/gtest.h>

namespace {

// Not using a map<> or unordered_map<> because we'd need to use some (possibly duplicate) property as the key
template <typename T> using ServerProperty = std::pair<std::string, T>;
template <typename T> using ServerProperties = std::vector<ServerProperty<T>>;

template <typename TDestination, typename TSource>
ServerProperties<TDestination> ConvertServerProperties(const ServerProperties<TSource>& source, const std::function<TDestination(const TSource&)>& convertValue) {
  ServerProperties<TDestination> result;
  result.reserve(source.size());
  std::transform(source.begin(), source.end(), std::back_inserter(result), [convertValue](const ServerProperty<TSource>& property) {
    return std::make_pair(property.first, convertValue(property.second));
    });
  return result;
}

template <typename T, typename Enable = void>
struct UniqueVerification;

template <>
struct UniqueVerification<std::string> {
  static void Perform(const std::string& description, ServerProperties<std::string> properties) {
    auto end = properties.end();
    for (auto i = properties.begin(); i != end; ++i) {
      auto j = i;
      for (++j; j != end; ++j) {
        EXPECT_NE(i->second, j->second) << i->first << " and " << j->first << " have the same " << description << ": \"" << i->second << '"';
      }
    }
  }
};

template <typename T>
struct UniqueVerification<T, std::enable_if_t<std::is_enum_v<T>>> {
  static void Perform(const std::string& description, ServerProperties<T> properties) {
    auto stringified = ConvertServerProperties<std::string, T>(properties, [](const T& value) {return std::to_string(pep::ToUnderlying(value)); });
    return UniqueVerification<std::string>::Perform(description, std::move(stringified));
  }
};

template <typename T>
struct UniqueVerification<std::optional<T>> {
  static void Perform(const std::string& description, ServerProperties<std::optional<T>> properties) {
    auto removed = std::remove_if(properties.begin(), properties.end(), [](const ServerProperty<std::optional<T>>& property) {
      return !property.second.has_value();
      });
    properties.erase(removed, properties.end());
    auto nonnull = ConvertServerProperties<T, std::optional<T>>(properties, [](const std::optional<T>& optional) -> T {return *optional; });
    return UniqueVerification<T>::Perform(description, std::move(nonnull));
  }
};

template <typename T>
void VerifyServerMethodProducesUniqueProperties(const std::vector<pep::ServerTraits>& servers, const std::string& property, T (pep::ServerTraits::*method)() const) {
  using Plain = std::remove_const_t<std::remove_reference_t<T>>;
  ServerProperties<Plain> properties;
  std::transform(servers.begin(), servers.end(), std::back_inserter(properties), [method](const pep::ServerTraits& server) {
    return std::make_pair(server.description(), (server.*method)());
    });
  UniqueVerification<Plain>::Perform(property, std::move(properties));
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
  VerifyServerMethodProducesUniqueProperties(servers, "enrolls as", &pep::ServerTraits::enrollsAs);
  VerifyServerMethodProducesUniqueProperties(servers, "enrollment subject", &pep::ServerTraits::enrollmentSubject);
}
