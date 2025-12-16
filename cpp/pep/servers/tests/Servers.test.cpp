#include <gtest/gtest.h>
#include <pep/client/Client.hpp>
#include <pep/async/IoContextThread.hpp>
#include <pep/async/RxConcatenateStrings.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/auth/OAuthToken.hpp>
#include <pep/servers/tests/Common.hpp>

#include <fstream>

#include <boost/asio/io_context.hpp>
#include <boost/algorithm/hex.hpp>
#include <nlohmann/json.hpp>
#include <rxcpp/operators/rx-concat.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>


namespace {

using namespace pep::serverstest;

class ServersTest : public ::testing::Test {
protected:
  static std::shared_ptr<pep::Client> mClient;
  static std::shared_ptr<boost::asio::io_context> mIoContext;
  static std::shared_ptr<pep::IoContextThread> mIoContextThread;
  static bool mKeepRunning;

  static void SetUpTestSuite() {
    if (mClient == nullptr) {
      auto oauthTokenSecretJson = nlohmann::json::parse(std::ifstream(constants::configDir / "keyserver/OAuthTokenSecret.json"));
      auto oauthTokenSecret = boost::algorithm::unhex(oauthTokenSecretJson["OAuthTokenSecret"].get<std::string>());
      auto now = pep::TimeNow<std::chrono::sys_seconds>();
      auto dataAdminToken = pep::OAuthToken::Generate(oauthTokenSecret, "Data Administrator", "Data Administrator", now, now + std::chrono::seconds(3600)).getSerializedForm();
      auto accessAdminToken = pep::OAuthToken::Generate(oauthTokenSecret, "Access Administrator", "Access Administrator", now, now + std::chrono::seconds(3600)).getSerializedForm();
      auto userToken = pep::OAuthToken::Generate(oauthTokenSecret, "PepTest", "PepTest", now, now + std::chrono::seconds(3600)).getSerializedForm();

      mIoContext = std::make_shared<boost::asio::io_context>();
      mIoContextThread = std::make_shared<pep::IoContextThread>(mIoContext, &mKeepRunning);
      mClient = pep::Client::OpenClient(pep::Configuration::FromFile("ClientConfig.json"), mIoContext, false);
      mClient->enrollUser(dataAdminToken).as_blocking().last();
      mClient->getAccessManagerProxy()->amaCreateColumn("Test.Servers.Data").as_blocking().last();
      mClient->getAccessManagerProxy()->amaCreateColumnGroup("Test.Servers").as_blocking().last();
      mClient->getAccessManagerProxy()->amaAddColumnToGroup("Test.Servers.Data", "Test.Servers").as_blocking().last();
      mClient->enrollUser(accessAdminToken).as_blocking().last();
      mClient->getAccessManagerProxy()->amaCreateColumnGroupAccessRule("Test.Servers", "PepTest", "read").as_blocking().last();
      mClient->getAccessManagerProxy()->amaCreateColumnGroupAccessRule("Test.Servers", "PepTest", "write").as_blocking().last();
      mClient->getAccessManagerProxy()->amaCreateGroupAccessRule("*", "PepTest", "access").as_blocking().last();
      mClient->getAccessManagerProxy()->amaCreateGroupAccessRule("*", "PepTest", "enumerate").as_blocking().last();
      mClient->enrollUser(userToken).as_blocking().last();
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
      mClient->retrieveData(
mClient->enumerateDataByIds(
            {dataStorageResult.mIds.at(0)},
              ticket.getTicket()
          ).concat(),
          ticket.getTicket()
      )
      .concat()
      .map([](pep::RetrievePage page) {
        if (page.fileIndex != 0) {
          throw std::runtime_error("Unexpected file index");
        }
        return std::move(page.content);
      })
      .op(pep::RxConcatenateStrings())
      .as_blocking().first();
  ASSERT_EQ(retrieveResult, testData);
}

}
