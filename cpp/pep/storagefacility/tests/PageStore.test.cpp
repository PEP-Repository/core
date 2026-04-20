#include <pep/storagefacility/PageStore.hpp>
#include <pep/storagefacility/tests/sftest.hpp>

#include <gtest/gtest.h>

#include <pep/application/Application.hpp>
#include <pep/async/tests/RxTestUtils.hpp>
#include <pep/utils/Defer.hpp>
#include <pep/utils/Random.hpp>
#include <pep/networking/EndPoint.PropertySerializer.hpp>
#include <pep/storagefacility/S3Credentials.PropertySerializer.hpp>

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

  boost::property_tree::ptree s3Conf;
  SerializeProperties(s3Conf, "EndPoint", EndPoint(envs.host, envs.port, envs.expect_common_name));
  SerializeProperties(s3Conf, "Credentials", s3::Credentials{
    .accessKey = envs.s3_access_key,
    .secret = envs.s3_secret_key,
    .service = envs.s3_service_name,
  });
  s3Conf.put("CaCertificateFile", envs.GetCaCertFilepath().string());
  s3Conf.put("WriteToBucket", envs.s3_test_bucket);
  SerializeProperties(s3Conf, "ReadFromBuckets", std::vector{envs.s3_test_bucket, envs.s3_test_bucket2});

  boost::property_tree::ptree pageStoreConf;
  pageStoreConf.put_child("Local", s3Conf);

  std::shared_ptr<PageStore> store = PageStore::Create(
    io_context,
    std::shared_ptr<prometheus::Registry>(), // intentionally null
    Configuration::FromPtree(s3Conf)
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
