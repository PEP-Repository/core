#include <boost/algorithm/hex.hpp>

#include <gtest/gtest.h>

#include <pep/utils/Md5.hpp>
#include <pep/utils/Sha.hpp>

namespace {

template <typename TCreateHasher, typename... TValues>
void TestHasher(const std::string& name, const TCreateHasher& createHasher, const std::string& expected, TValues&&... values) {
  auto hasher = createHasher();
  using Hasher = typename std::remove_reference<decltype(*hasher)>::type;
  static_assert(std::is_same<std::string, typename Hasher::Hash>::value, "Only usable for hasher types that produce a string value");

  auto hash = hasher->digest(std::forward<TValues>(values)...);
  auto formatted = boost::algorithm::hex(hash);

  ASSERT_EQ(formatted, expected) << name << " hashing produced " << formatted << " instead of expected value";
}

TEST(Hashing, MD5) {
  TestHasher("MD5",
    []() {return std::make_unique<pep::Md5>(); },
    "E319B7E48050C03F5E4A6F97D55DB661", // Expected value produced by old implementation: see https://gitlab.pep.cs.ru.nl/pep/core/-/blob/eb942ae30fa51345ffcfb64f202b3373095cdec5/core/MiscCrypto.hpp#L89
    std::string("I have seen things you people wouldn't believe."),
    std::string("Attack ships on fire off the shoulder of Orion."));
}

TEST(Hashing, SHA256) {
  TestHasher("SHA-256",
    []() {return std::make_unique<pep::Sha256>(); },
    "A616D1A0810115D5338F86266B17A206C0709F68DA9A7DB0C0EDF362C7196D29", // Expected value produced by legacy OpenSSL API: see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2107
    std::string("Quite an experience to live in fear, isn't it? That's what it is to be a slave."));
}

TEST(Hashing, SHA512) {
  TestHasher("SHA-512",
    []() {return std::make_unique<pep::Sha512>(); },
    "349E4FCEC4D9F29461DFCA90FAA63EEA1408EAA1C9807626C68B7122CFE8DE82364F92916404B2BCD5342D6EE784A1E3BBF7109DD60690C6EB77903CD175A4F5", // Expected value produced by legacy OpenSSL API: see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2107
    std::string("You look down and see a tortoise, Leon. It's crawling toward you."));
}

}
