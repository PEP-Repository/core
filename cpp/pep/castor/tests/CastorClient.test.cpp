#include <chrono>

#include <boost/property_tree/json_parser.hpp>

#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-any.hpp>
#include <rxcpp/operators/rx-timeout.hpp>
#include <rxcpp/operators/rx-merge.hpp>
#include <rxcpp/operators/rx-filter.hpp>

#include <gtest/gtest.h>

#include <pep/castor/CastorClient.hpp>
#include <pep/castor/Study.hpp>
#include <pep/castor/tests/FakeCastorApi.hpp>
#include <pep/castor/tests/Responses.hpp>
#include <pep/crypto/Timestamp.hpp>

namespace {

const auto TIMEOUT = std::chrono::seconds(5);

// Implemented as "invoke this lambda" so that a semicolon is required: ASSERT_THROW_WITH_MESSAGE(mycode(), std::runtime_error);
#define ASSERT_THROW_WITH_MESSAGE(statement, expected_exception) \
[&]() { \
  bool exception_thrown = false; \
  try { statement; } \
  catch (const expected_exception&) { exception_thrown = true; } \
  catch (const std::exception& ex ) { FAIL() << "Expected: " << #expected_exception << " thrown in " #statement << std::endl << \
    "Actual: Different type is thrown with message: " << ex.what(); } \
  catch (...) { FAIL() << "Expected: " << #expected_exception << " thrown in " #statement << std::endl << \
    "Actual: Different type is thrown"; } \
  if(!exception_thrown) { \
    FAIL() << "Expected: " << #expected_exception << " thrown in " #statement << std::endl << \
    "Actual: nothing is thrown"; \
  } \
}()

using namespace pep;
using namespace pep::castor;
using namespace pep::tests;

using CastorClientTest = FakeCastorTest;

TEST_F(CastorClientTest, Authentication) {
  options->responses.emplace("/api/study?page_size=1000", FakeCastorApi::Response(RESPONSE_STUDIES));

  options->authenticated = false;
  ASSERT_THROW_WITH_MESSAGE(
    castorConnection->getStudies().timeout(TIMEOUT).as_blocking()
    .subscribe_with_rethrow([](std::shared_ptr<Study> study) {
      FAIL() << "Received a study without being authenticated.";
      })
    , CastorException);

  castorConnection->reauthenticate();
  bool authenticationError = castorConnection->authenticationStatus()
                             .timeout(TIMEOUT)
  .map([](AuthenticationStatus status) {
    return status.state;
  })
  .contains(AUTHENTICATION_ERROR)
  .as_blocking().first();
  ASSERT_TRUE(authenticationError) << "Castor authentication did not result in an error";

  options->authenticated = true;
  castorConnection->reauthenticate();
  bool authenticated = castorConnection->authenticationStatus()
                       .timeout(TIMEOUT)
  .map([](AuthenticationStatus status) {
    return status.state;
  })
  .contains(AUTHENTICATED)
  .as_blocking().first();
  ASSERT_TRUE(authenticated) << "Castor authentication did not succeed";


  ASSERT_NO_THROW(
    castorConnection->getStudies().as_blocking()
  .subscribe_with_rethrow([](std::shared_ptr<Study> study) {}));
}

TEST_F(CastorClientTest, SendCastorRequest) {

  ASSERT_THROW_WITH_MESSAGE(
    castorConnection->sendCastorRequest(castorConnection->makeGet("not/existing/path")).as_blocking()
    .subscribe_with_rethrow([](JsonPtr json) {
      FAIL() << "Received a response for a not existing path.";
      })
    , CastorException
  );

  options->responses.emplace("/api/some/path?page_size=1000", FakeCastorApi::Response("Incorrect json"));
  ASSERT_THROW_WITH_MESSAGE(
    castorConnection->sendCastorRequest(castorConnection->makeGet("some/path")).as_blocking()
    .subscribe_with_rethrow([](JsonPtr json) {
      FAIL() << "Received a response for incorrect json";
      })
    , boost::property_tree::json_parser_error);

  options->responses.emplace("/api/another/path?page_size=1000", FakeCastorApi::Response("{\"key\": \"value\"}"));
  JsonPtr result = castorConnection->sendCastorRequest(castorConnection->makeGet("another/path")).as_blocking().first();
  ASSERT_EQ(result->get<std::string>("key"), "value");

  int count = castorConnection->sendCastorRequest(castorConnection->makeGet("another/path")).as_blocking().count();
  ASSERT_EQ(count, 1);
}

TEST_F(CastorClientTest, GetStudies) {
  options->responses.emplace("/api/study?page_size=1000", FakeCastorApi::Response(RESPONSE_STUDIES));

  std::shared_ptr<Study> study = castorConnection->getStudies().as_blocking().first();
  ASSERT_EQ(study->getId(), "14F7C4E0-0FA5-C430-B7A2-9ECCB6271FA6");

  int count = castorConnection->getStudies().as_blocking().count();
  ASSERT_EQ(count, 23);

  ASSERT_THROW_WITH_MESSAGE(
    castorConnection->getStudyBySlug("NotExisting").as_blocking()
    .subscribe_with_rethrow([](std::shared_ptr<Study> study) {
      FAIL() << "Received a study for not existing slug.";
      })
    , rxcpp::empty_error);

  ASSERT_EQ(castorConnection->getStudyBySlug("pep-hq1").as_blocking().first()->getId(), "22B35F42-DB4F-09E4-F5F0-71CDCF4F34ED");

  ASSERT_EQ(castorConnection->getStudyBySlug("pep-hq1").as_blocking().count(), 1);
}

TEST_F(CastorClientTest, MultiPage) {
  options->responses.emplace("/api/study?page_size=1000", FakeCastorApi::Response(RESPONSE_STUDIES_MULTIPAGE_PAGE1));
  options->responses.emplace("/api/study?page=2&page_size=1000", FakeCastorApi::Response(RESPONSE_STUDIES_MULTIPAGE_PAGE2));

  std::shared_ptr<Study> study = castorConnection->getStudies().as_blocking().first();
  ASSERT_EQ(study->getId(), "14F7C4E0-0FA5-C430-B7A2-9ECCB6271FA6");

  int count = castorConnection->getStudies().as_blocking().count();
  ASSERT_EQ(count, 46);

  bool containsFromPage1 = castorConnection->getStudies().map([](std::shared_ptr<Study> study) {
    return study->getId();
  }).contains(std::string("14F7C4E0-0FA5-C430-B7A2-9ECCB6271FA6")).as_blocking().first();
  ASSERT_TRUE(containsFromPage1) << "getStudies did not return a result containing a study from the first page";

  bool containsFromPage2 = castorConnection->getStudies().map([](std::shared_ptr<Study> study) {
    return study->getId();
  }).contains(std::string("24F7C4E0-0FA5-C430-B7A2-9ECCB6271FA6")).as_blocking().first();
  ASSERT_TRUE(containsFromPage2) << "getStudies did not return a result containing a study from the second page";
}


TEST_F(CastorClientTest, RateLimited) {
  //First, make the FakeCastorApi return a "Too Many Requests" response, telling the client to retry after 2 seconds
  auto after = Timestamp::FromTimeT(std::time(nullptr) + 2).toString();
  options->responses.emplace("/api/throttle?page_size=1000",
    FakeCastorApi::Response(
      R"({"success":false,"errors":[{"id":"fa420c23","code":"CODE_QUOTA_EXCEEDED","message":"Too many requests, retry after: )" + after + R"(","data":[]}]})",
      "429 Too Many Requests"));
  //Then, update the resonse after 1 second
  auto updateResponseObservable = rxcpp::rxs::timer(std::chrono::seconds(1)).map([opts = options](auto) -> JsonPtr {
    opts->responses.at("/api/throttle?page_size=1000") = FakeCastorApi::Response("{\"key\": \"value\"}");
    return nullptr;
  });
  auto castorObservable = castorConnection->sendCastorRequest(castorConnection->makeGet("throttle"));
  //Merge both observables, so they are both subscribed to, and therefore executed simultaneously
  JsonPtr result = castorObservable.merge(updateResponseObservable)
    .filter([](JsonPtr v){ return v != nullptr; })
    .as_blocking().first();
  //Check that we got the response that was only available after the original request was made.
  ASSERT_EQ(result->get<std::string>("key"), "value");
}

}
