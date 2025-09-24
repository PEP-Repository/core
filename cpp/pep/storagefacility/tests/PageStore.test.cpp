#include <pep/storagefacility/PageStore.hpp>
#include <pep/storagefacility/tests/sftest.hpp>

#include <gtest/gtest.h>

#include <pep/application/Application.hpp>
#include <pep/async/tests/RxTestUtils.hpp>
#include <pep/utils/Defer.hpp>
#include <pep/utils/Random.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/property_tree/json_parser.hpp>

using namespace pep;

// This test requires an S3 server (such as minio or s3proxy) to be running
// at https://localhost:9000, or at the location specified
// with the PEP_S3_{HOST,PORT} environmental variables.
//
// If you get an "unable to get local issuer certificate error",
// then the PEP_ROOT_CA environmental variable might not be set (correctly).
namespace {

TEST(PageStore, basic) {
  auto io_context = std::make_shared<boost::asio::io_context>();
  // Run the I/O service one final time after all other PEP_DEFER invocations have scheduled their I/O cleanup jobs (i.e. TLS shutdowns)
  PEP_DEFER(io_context->run());

  sftest::Envs envs; // filled by constructor

  // create json config file
  std::stringstream ss;

  ss
    << "{\n"
    << "  \"Type\" : \"s3\",\n"
    << "  \"EndPoint\" : { \n"
    << "    \"Address\" : " << std::quoted(envs.host) << ",\n"
    << "    \"Port\" : " << std::quoted(std::to_string(envs.port)) << ",\n"
    << "    \"Name\" : "
    << std::quoted(envs.expect_common_name) << "\n"
    << "  },\n"
    << "  \"Credentials\" : { \n"
    << "    \"AccessKey\" : " << std::quoted(envs.s3_access_key) << ",\n"
    << "    \"Secret\" : " << std::quoted(envs.s3_secret_key) << ",\n"
    << "    \"Service\" : " << std::quoted(envs.s3_service_name) << "\n"
    << "  },\n"
    << "  \"Ca-Cert-Path\" : "
    << std::quoted(envs.GetCaCertFilepath().string()) << ",\n"
    << "  \"Write-To-Bucket\" : " << std::quoted(envs.s3_test_bucket) << ",\n"
    << "  \"Read-From-Buckets\" : [\n"
    << "    " << std::quoted(envs.s3_test_bucket) << ",\n"
    << "    " << std::quoted(envs.s3_test_bucket2) << "\n"
    << "  ]\n"
    << "}\n";

  auto config = std::make_shared<Configuration>(
    Configuration::FromStream(ss));

  std::shared_ptr<PageStore> store = PageStore::Create(
    io_context,
    std::shared_ptr<prometheus::Registry>(), // intentionally null
    config
  );
  PEP_DEFER(store.reset());

  std::shared_ptr<s3::Client> direct_conn
    = envs.CreateS3Client(io_context);
  direct_conn->start();
  std::cerr << "Connecting to " << envs.s3_host_type << " S3 host at " << envs.host << ":" << envs.port << '.'
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
    direct_conn->putObject(path, envs.s3_test_bucket2, data2))->size(), 1);

  // now store->get(path) should yield data2
  {
    auto results = testutils::exhaust<std::shared_ptr<std::string>>(
      *io_context, store->get(path));
    ASSERT_EQ(results->size(), 1);
    EXPECT_EQ(*((*results)[0]), data2);
  }

  // if we put data under path in s3_test_bucket, ...
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

}
