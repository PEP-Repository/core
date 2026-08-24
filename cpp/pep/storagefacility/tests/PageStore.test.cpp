#include <pep/storagefacility/PageStore.hpp>
#include <pep/storagefacility/tests/sftest.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include <pep/application/Application.hpp>
#include <pep/async/tests/RxTestUtils.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/Defer.hpp>
#include <pep/utils/Random.hpp>
#include <pep/networking/EndPoint.PropertySerializer.hpp>
#include <pep/storagefacility/S3Credentials.PropertySerializer.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <pep/utils/MiscUtil.hpp>

using namespace pep;
using namespace std::ranges;

// The tests in the "S3PageStore" and "S3PageStoreMultiHost" suites require an S3 server (such as
// minio or s3proxy) to be running at https://localhost:9000, or at the location specified with the
// PEP_S3_{HOST,PORT} environmental variables. The "S3PageStoreMultiHost" suite needs a second S3
// server as well, at the location specified with the PEP_S3_B_* variables: see sftest.hpp.
//
// If you get an "unable to get local issuer certificate error",
// then the PEP_ROOT_CA environmental variable might not be set (correctly).
//
// The tests in the "S3PageStoreConfig" suite need no S3 server at all.
namespace {

// Identifies a bucket in a page store configuration, i.e. an entry of "S3.ReadFromBuckets"
// or the value of "S3.WriteToBucket".
struct BucketRef {
  std::string name;
  std::string hostId;
};

boost::property_tree::ptree BucketConfig(const BucketRef& bucket) {
  boost::property_tree::ptree result;
  result.put("Name", bucket.name);
  result.put("HostId", bucket.hostId);
  return result;
}

boost::property_tree::ptree HostConfig(const sftest::S3HostEnvs& host) {
  boost::property_tree::ptree result;
  SerializeProperties(result, "EndPoint", EndPoint(host.host, host.port, host.expectCommonName));
  SerializeProperties(result, "Credentials", host.credentials);
  result.put("CaCertificateFile", host.caCertPath.string());
  result.put("UseHttps", host.useHttps);
  return result;
}

// Produces the configuration for a page store that stores its pages on (one or more) S3 hosts.
// Every bucket must refer to a host by the key that that host has in "hosts".
boost::property_tree::ptree PageStoreConfig(
    const std::unordered_map<std::string, sftest::S3HostEnvs>& hosts,
    const std::vector<BucketRef>& readBuckets,
    const BucketRef& writeBucket) {

  boost::property_tree::ptree hostsConfig;
  for (const auto& [id, host] : hosts) {
    hostsConfig.push_back({id, HostConfig(host)});
  }

  std::vector<boost::property_tree::ptree> readBucketConfigs;
  for (const BucketRef& bucket : readBuckets) {
    readBucketConfigs.push_back(BucketConfig(bucket));
  }

  boost::property_tree::ptree s3Config;
  s3Config.put_child("Hosts", hostsConfig);
  SerializeProperties(s3Config, "ReadFromBuckets", readBucketConfigs);
  s3Config.put_child("WriteToBucket", BucketConfig(writeBucket));

  boost::property_tree::ptree result;
  result.put_child("S3", s3Config);
  return result;
}

std::shared_ptr<PageStore> CreatePageStore(
    std::shared_ptr<boost::asio::io_context> io_context,
    const boost::property_tree::ptree& config) {

  return PageStore::Create(
    std::move(io_context),
    std::shared_ptr<prometheus::Registry>(), // intentionally null
    Configuration::FromPtree(config));
}


TEST(S3PageStore, basic) {
  auto io_context = std::make_shared<boost::asio::io_context>();
  // Run the I/O service one final time after all other PEP_DEFER invocations have scheduled their I/O cleanup jobs (i.e. TLS shutdowns)
  PEP_DEFER(io_context->run());

  sftest::Envs envs; // filled by constructor

  std::shared_ptr<PageStore> store = CreatePageStore(io_context, PageStoreConfig(
    {{"s3test", envs.hostA}},
    {
      {.name = envs.s3TestBucket, .hostId = "s3test"},
      {.name = envs.s3TestBucket2, .hostId = "s3test"},
    },
    {.name = envs.s3TestBucket, .hostId = "s3test"}));
  PEP_DEFER(store.reset());

  std::shared_ptr<s3::Client> direct_conn
    = sftest::CreateS3Client(io_context, envs.hostA);
  direct_conn->start();
  std::cerr << "Connecting to " << envs.s3HostType << " S3 host at "
    << envs.hostA.host << ":" << envs.hostA.port << '.'
    << " If this test seems to hang, please check if someone's listening." << std::endl;
  PEP_DEFER(direct_conn->shutdown());

  std::string path = boost::algorithm::hex(RandomString(5));
  std::string data = RandomString(10);
  std::string data2 = RandomString(10);

  // store->get(path) should return nothng, since that object doesn't exist:
  EXPECT_TRUE(testutils::exhaust<std::shared_ptr<std::string>>(
    *io_context, store->get(path))->empty());

  // we put data2 under at "path" in the backup bucket s3_test_bucket2
  EXPECT_EQ(testutils::exhaust<std::string>(*io_context,
    direct_conn->putObject(path, envs.s3TestBucket2, data2))->size(), 1);

  // now store->get(path) should yield data2
  {
    auto results = testutils::exhaust<std::shared_ptr<std::string>>(
      *io_context, store->get(path));
    ASSERT_EQ(results->size(), 1);
    EXPECT_EQ(*((*results)[0]), data2);
  }

  // if we put data under path in s3TestBucket, ...
  EXPECT_EQ(testutils::exhaust<std::string>(
    *io_context, store->put(path, data))->size(), 1);

  // ... store->get(path) should now yield data.
  {
    auto results = testutils::exhaust<std::shared_ptr<std::string>>(
      *io_context, store->get(path));
    ASSERT_EQ(results->size(), 1);
    EXPECT_EQ(*((*results)[0]), data);
  }

}


// Tests for page stores whose buckets are spread over multiple S3 hosts.
//
// Both hosts serve a bucket with the same name (envs.s3TestBucket), so a page can only end up in
// the right place if the page store honours the "HostId" of its buckets. The hosts also have
// different credentials, so a request that is sent to the wrong host is rejected outright.
class S3PageStoreMultiHost : public ::testing::Test {
protected:
  static const inline std::string hostAId = "s3test-a", hostBId = "s3test-b";

  std::shared_ptr<boost::asio::io_context> ioContext;
  sftest::Envs envs; // filled by constructor
  // Connections that bypass the page store, so that we can see what it stored where.
  std::shared_ptr<s3::Client> directToA, directToB;
  // The path of the page under test. Randomized so that repeated runs don't interfere.
  std::string path;

  void SetUp() override {
    ioContext = std::make_shared<boost::asio::io_context>();
    directToA = sftest::CreateS3Client(ioContext, envs.hostA);
    directToA->start();
    directToB = sftest::CreateS3Client(ioContext, envs.hostB);
    directToB->start();
    path = boost::algorithm::hex(RandomString(5));

    std::cerr << "Connecting to S3 hosts at " << envs.hostA.host << ":" << envs.hostA.port
      << " and " << envs.hostB.host << ":" << envs.hostB.port << '.'
      << " If this test seems to hang, please check if someone's listening." << std::endl;
  }

  void TearDown() override {
    directToA->shutdown();
    directToB->shutdown();
    // Run the I/O service one final time, so that the cleanup jobs (i.e. TLS shutdowns) scheduled
    // by the shutdowns above can complete.
    ioContext->run();
  }

  // Creates a page store with the test bucket on both hosts, reading from those buckets in the
  // specified order, and writing to the one on the specified host.
  std::shared_ptr<PageStore> createStore(
      const std::vector<std::string>& readFromHostIds,
      const std::string& writeToHostId) {

    std::vector<BucketRef> readBuckets;
    for (const std::string& hostId : readFromHostIds) {
      readBuckets.push_back({.name = envs.s3TestBucket, .hostId = hostId});
    }

    return CreatePageStore(ioContext, PageStoreConfig(
      {{hostAId, envs.hostA}, {hostBId, envs.hostB}},
      readBuckets,
      {.name = envs.s3TestBucket, .hostId = writeToHostId}));
  }

  void putDirectly(s3::Client& host, const std::string& data) {
    ASSERT_EQ(testutils::exhaust<std::string>(
      *ioContext, host.putObject(path, envs.s3TestBucket, data))->size(), 1);
  }

  // Returns the contents of the page, or an empty optional if the host doesn't have it.
  std::optional<std::string> getDirectly(s3::Client& host) {
    return single(testutils::exhaust<std::shared_ptr<std::string>>(
      *ioContext, host.getObject(path, envs.s3TestBucket)));
  }

  // Returns the contents of the page, or an empty optional if the store doesn't have it.
  std::optional<std::string> getFromStore(PageStore& store) {
    return single(testutils::exhaust<std::shared_ptr<std::string>>(
      *ioContext, store.get(path)));
  }

private:
  static std::optional<std::string> single(const std::shared_ptr<std::vector<std::shared_ptr<std::string>>>& results) {
    return GetOptionalValue(RangeToOptional(*results), [](const auto& ptr) { return *ptr; });
  }
};

// A page must be written to the host of the "WriteToBucket", and to that host only.
TEST_F(S3PageStoreMultiHost, putsPageOnWriteBucketHost) {
  std::shared_ptr<PageStore> store = createStore({hostBId, hostAId}, hostBId);
  std::string data = RandomString(10);

  EXPECT_EQ(testutils::exhaust<std::string>(
    *ioContext, store->put(path, data))->size(), 1);

  EXPECT_EQ(getDirectly(*directToB), data);
  EXPECT_EQ(getDirectly(*directToA), std::nullopt);
}

// A page that's only present on the second host must still be found.
TEST_F(S3PageStoreMultiHost, getsPageFromOtherHost) {
  std::shared_ptr<PageStore> store = createStore({hostBId, hostAId}, hostBId);
  std::string data = RandomString(10);

  // Put the page on host A only, which is the last bucket that the store reads from.
  putDirectly(*directToA, data);
  ASSERT_EQ(getDirectly(*directToB), std::nullopt);

  EXPECT_EQ(getFromStore(*store), data);
}

// When multiple hosts have a page with the same path, the bucket that's listed first in
// "ReadFromBuckets" wins.
TEST_F(S3PageStoreMultiHost, prefersFirstReadBucket) {
  std::string dataOnA = RandomString(10);
  std::string dataOnB = RandomString(10);
  putDirectly(*directToA, dataOnA);
  putDirectly(*directToB, dataOnB);

  {
    std::shared_ptr<PageStore> store = createStore({hostBId, hostAId}, hostBId);
    EXPECT_EQ(getFromStore(*store), dataOnB);
  }
  {
    std::shared_ptr<PageStore> store = createStore({hostAId, hostBId}, hostAId);
    EXPECT_EQ(getFromStore(*store), dataOnA);
  }
}


// Tests for the handling of the page store's configuration, which contact no S3 host at all.
//
// Creating a page store does create (and start) clients for the configured hosts, but those only
// perform I/O once the io_context is run, which these tests deliberately never do.
class S3PageStoreConfig : public ::testing::Test {
protected:
  std::shared_ptr<boost::asio::io_context> ioContext;
  // Hosts that are never contacted, so their addresses don't have to lead anywhere, and their
  // credentials don't have to be valid. They use plaintext HTTP, so that creating a client for
  // them doesn't require certificates either.
  sftest::S3HostEnvs someHost{
    .host = "s3.example.com",
    .port = 9000,
    .useHttps = false,
    .credentials = {.accessKey = "SomeAccessKey", .secret = "SomeSecret"},
  };
  sftest::S3HostEnvs anotherHost{
    .host = "s3.example.org",
    .port = 9000,
    .useHttps = false,
    .credentials = {.accessKey = "AnotherAccessKey", .secret = "AnotherSecret"},
  };

  void SetUp() override {
    ioContext = std::make_shared<boost::asio::io_context>();
  }

  std::shared_ptr<PageStore> createStore(const boost::property_tree::ptree& config) {
    return CreatePageStore(ioContext, config);
  }

  // Checks that a page store cannot be created from the given configuration, and that the reason
  // given mentions "expectedInMessage".
  void expectCreationFailure(
      const boost::property_tree::ptree& config,
      const std::string& expectedInMessage) {

    using namespace testing;
    EXPECT_THAT([&] { createStore(config); },
      ThrowsMessage<std::invalid_argument>(HasSubstr(expectedInMessage)));
  }
};

TEST_F(S3PageStoreConfig, requiresBucketsToReadFrom) {
  boost::property_tree::ptree config = PageStoreConfig(
    {{"host", someHost}},
    {{.name = "bucket", .hostId = "host"}},
    {.name = "bucket", .hostId = "host"});
  config.get_child("S3").erase("ReadFromBuckets");

  expectCreationFailure(config, "no buckets to read from");
}

TEST_F(S3PageStoreConfig, requiresWriteBucketToBeReadFrom) {
  expectCreationFailure(PageStoreConfig(
    {{"host", someHost}},
    {{.name = "bucket", .hostId = "host"}},
    {.name = "otherBucket", .hostId = "host"}),
    "writing to a bucket we're not reading from");
}

// Buckets with the same name on different hosts are different buckets.
TEST_F(S3PageStoreConfig, distinguishesBucketsByHost) {
  expectCreationFailure(PageStoreConfig(
    {{"host", someHost}, {"otherHost", anotherHost}},
    {{.name = "bucket", .hostId = "host"}},
    {.name = "bucket", .hostId = "otherHost"}),
    "writing to a bucket we're not reading from");
}

TEST_F(S3PageStoreConfig, requiresBucketsToReferToKnownHost) {
  expectCreationFailure(PageStoreConfig(
    {{"host", someHost}},
    {
      {.name = "bucket", .hostId = "host"},
      {.name = "bucket", .hostId = "typo"},
    },
    {.name = "bucket", .hostId = "host"}),
    "\"typo\"");
}

TEST_F(S3PageStoreConfig, requiresNonzeroNumberOfConnections) {
  boost::property_tree::ptree config = PageStoreConfig(
    {{"host", someHost}},
    {{.name = "bucket", .hostId = "host"}},
    {.name = "bucket", .hostId = "host"});
  config.put("S3.Hosts.host.Connections", 0);

  expectCreationFailure(config, "connections");
}

}
