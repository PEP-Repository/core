#include <gtest/gtest.h>
#include <pep/client/Client.hpp>
#include <pep/async/IoContextThread.hpp>
#include <pep/async/RxConcatenateStrings.hpp>
#include <pep/utils/Configuration.hpp>

#include <boost/asio/io_context.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>

namespace {

const std::string DATA_ADMIN_TOKEN = "ewogICAgInN1YiI6ICJEYXRhIEFkbWluaXN0cmF0b3IiLAogICAgImdyb3VwIjogIkRhdGEgQWRtaW5pc3RyYXRvciIsCiAgICAiaWF0IjogIjE2NTQ4NjI2MzYiLAogICAgImV4cCI6ICI0Nzk2NjY1MjAwIgp9Cg.3_jQO5IpBYv5TwmFimOAuJRjLXa7idsJAm0I-b08GI8";
const std::string ACCESS_ADMIN_TOKEN = "ewogICAgInN1YiI6ICJBY2Nlc3MgQWRtaW5pc3RyYXRvciIsCiAgICAiZ3JvdXAiOiAiQWNjZXNzIEFkbWluaXN0cmF0b3IiLAogICAgImlhdCI6ICIxNjU0ODYyNjIyIiwKICAgICJleHAiOiAiNDc5NjY2NTIwMCIKfQo.whgTe3qHtHLRUbTYzyhzECyG9s0Zay8V6XSzE5oQb_I";
const std::string USER_TOKEN = "ewogICAgInN1YiI6ICJQZXBUZXN0IiwKICAgICJncm91cCI6ICJQZXBUZXN0IiwKICAgICJpYXQiOiAiMTY1NDg2MjE1MCIsCiAgICAiZXhwIjogIjQ3OTY2NjUyMDAiCn0K.9eXY9BKQ2IibrC4-_xU4WUtprPWrp_lcxjKWfO8MTyc";

class ServersTest : public ::testing::Test {
protected:
  static std::shared_ptr<pep::Client> mClient;
  static std::shared_ptr<boost::asio::io_context> mIoContext;
  static std::shared_ptr<pep::IoContextThread> mIoContextThread;
  static bool mKeepRunning;

  static void SetUpTestSuite() {
    if (mClient == nullptr) {
      mIoContext = std::make_shared<boost::asio::io_context>();
      mIoContextThread = std::make_shared<pep::IoContextThread>(mIoContext, &mKeepRunning);
      mClient = pep::Client::OpenClient(pep::Configuration::FromFile("ClientConfig.json"), mIoContext, false);
      mClient->enrollUser(DATA_ADMIN_TOKEN).as_blocking().last();
      mClient->amaCreateColumn("Test.Servers.Data").as_blocking().last();
      mClient->amaCreateColumnGroup("Test.Servers").as_blocking().last();
      mClient->amaAddColumnToGroup("Test.Servers.Data", "Test.Servers").as_blocking().last();
      mClient->enrollUser(ACCESS_ADMIN_TOKEN).as_blocking().last();
      mClient->amaCreateColumnGroupAccessRule("Test.Servers", "PepTest", "read").as_blocking().last();
      mClient->amaCreateColumnGroupAccessRule("Test.Servers", "PepTest", "write").as_blocking().last();
      mClient->amaCreateGroupAccessRule("*", "PepTest", "access").as_blocking().last();
      mClient->amaCreateGroupAccessRule("*", "PepTest", "enumerate").as_blocking().last();
      mClient->enrollUser(USER_TOKEN).as_blocking().last();
    }
  }

  static void TearDownTestSuite() {
    mKeepRunning = false;
    mIoContext->stop();
    mIoContextThread->join();
  }
};

std::shared_ptr<pep::Client> ServersTest::mClient = nullptr;
std::shared_ptr<boost::asio::io_context> ServersTest::mIoContext = nullptr;
std::shared_ptr<pep::IoContextThread> ServersTest::mIoContextThread = nullptr;
bool ServersTest::mKeepRunning = true;

TEST_F(ServersTest, Enrollment) {
  ASSERT_TRUE(mClient->getEnrolled());
  ASSERT_EQ(mClient->getEnrolledUser(), "PepTest");
}

TEST_F(ServersTest, UpAndDownload) {
  const std::string testData = "Hello world!";
  auto pp = mClient->parsePPorIdentity("1111111111").as_blocking().first();

  pep::requestTicket2Opts reqTicketOpts;
  reqTicketOpts.columns = { "Test.Servers.Data" };
  reqTicketOpts.pps = { pp };
  reqTicketOpts.modes = { "read", "write" };
  auto ticket = mClient->requestTicket2(reqTicketOpts).as_blocking().first();

  pep::storeData2Opts storeDataOpts;
  storeDataOpts.ticket = std::make_shared<pep::IndexedTicket2>(ticket);
  auto dataStorageResult = mClient->storeData2(pp, "Test.Servers.Data", std::make_shared<std::string>(testData), {pep::MetadataXEntry::MakeFileExtension(".txt")}, storeDataOpts).as_blocking().first();
  ASSERT_EQ(dataStorageResult.mIds.size(), 1);
  auto retrieveResult =
      mClient->retrieveData2(
          mClient->getKeys(
              mClient->enumerateDataByIds({dataStorageResult.mIds.at(0)},
                  ticket.getTicket()).concat(),
              ticket.getTicket()
              ),
          ticket.getTicket())
      .concat()
      .map([](pep::RetrievePage page) {
        if (page.fileIndex != 0) {
          throw std::runtime_error("Unexpected file index");
        }
        return std::move(page.mContent);
      })
      .op(pep::RxConcatenateStrings())
      .as_blocking().first();
  ASSERT_EQ(retrieveResult, testData);
}

}
