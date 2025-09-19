#include <pep/storagefacility/S3Client.hpp>
#include <pep/storagefacility/tests/sftest.hpp>

#include <gtest/gtest.h>

#include <pep/utils/Exceptions.hpp>
#include <pep/application/Application.hpp>
#include <pep/utils/Random.hpp>
#include <pep/async/tests/RxTestUtils.hpp>
#include <pep/utils/Defer.hpp>

using namespace pep;
using namespace pep::s3;

// This test requires an S3 server (such as minio or s3proxy)
// to be running at https://localhost:9000, or at the location
// specified with the PEP_S3_{HOST,PORT} environmental variables,
// see also sftest.hpp.
//
// If you get an "unable to get local issuer certificate error",
// then the PEP_ROOT_CA environmental variable might not be set (correctly).
namespace {

  TEST(S3Client, putObject) {
    auto io_context = std::make_shared<boost::asio::io_context>();
    sftest::Envs envs; // fills itself with environment variables PEP_*

    std::shared_ptr<Client> client = envs.CreateS3Client(io_context);
    client->start();
    PEP_DEFER(client->shutdown(); io_context->run(););

    std::string data = RandomString(10);

    {
      auto results = testutils::exhaust<std::string>(*io_context,
        client->putObject("objectName", envs.s3_test_bucket, data));

      EXPECT_EQ(results->size(), 1);
    }

    {
      auto results = testutils::exhaust<std::shared_ptr<std::string>>(
          *io_context, client->getObject("objectName", envs.s3_test_bucket));

      EXPECT_EQ(results->size(), 1);
      EXPECT_EQ(*((*results)[0]), data);
    }

    {
      auto results = testutils::exhaust<std::shared_ptr<std::string>>(
          *io_context,
        client->getObject("AnObjectThatShouldnotExist", envs.s3_test_bucket));

      EXPECT_EQ(results->size(), 0);
    }

    EXPECT_ANY_THROW(testutils::exhaust<std::shared_ptr<std::string>>(
          *io_context,
        client->getObject("objectName", "myNonExistingBucket")));
  }

}
