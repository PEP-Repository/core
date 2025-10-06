#include <optional>

#include <pep/application/Application.hpp>
#include <pep/versioning/Version.hpp>
#include <pep/client/Client.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Sha.hpp>
#include <pep/utils/Platform.hpp>
#include <pep/storagefacility/Constants.hpp>
#include <pep/async/RxUtils.hpp>

#include <rxcpp/operators/rx-concat_map.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-merge.hpp>
#include <rxcpp/operators/rx-zip.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/asio/io_context.hpp>

using namespace pep;

namespace {

static const std::string LOG_TAG ("ClientTest");

class ClientTestApplication : public Application {
  std::optional<severity_level> consoleLogMinimumSeverityLevel() const override {
    return severity_level::warning;
  }

  using TestFunction = std::function<rxcpp::observable<bool>(std::shared_ptr<Client>)>;

  std::shared_ptr<Client> client;

  void shutdownClient() {
    if (client == nullptr) {
      throw std::runtime_error("Cannot shutdown client before it has been opened.");
    }
    client->shutdown().subscribe(
      [](FakeVoid unused) {},
      [this](std::exception_ptr ep) {
        client->getIoContext()->stop();
        LOG(LOG_TAG, error) << "Unexpected problem shutting down SSL streams: " + GetExceptionMessage(ep) << " | Forcefully shutting down.";
      },
      [] {});
  }

  int runTestFunction(const TestFunction& function) {
    Configuration config = this->loadMainConfigFile();
    auto io_context = std::make_shared<boost::asio::io_context>();
    client = Client::OpenClient(config, io_context);

    auto success = std::make_shared<bool>(true);

    function(client).subscribe(
      [success](bool entry) {if (!entry) *success = false; },
      [success, this](std::exception_ptr ep) {
        LOG(LOG_TAG, error) << "Exception occured: " << GetExceptionMessage(ep);
        *success = false;
        shutdownClient();
      },
      [this]() { shutdownClient(); }
    );

    client->getIoContext()->run();
    return (*success) ? 0 : -1;
  }

  std::string getDescription() const override {
    return "Tests PEP client functionality";
  }

  commandline::Parameters getSupportedParameters() const override {
    return Application::getSupportedParameters()
      + MakeConfigFileParameters(".", std::nullopt, true);
  }

  template <unsigned MODE>
  class ModeCommand : public commandline::ChildCommandOf<ClientTestApplication> {
  protected:
    virtual rxcpp::observable<bool> getTestResults(std::shared_ptr<Client> client) = 0;

    int execute() override {
      return this->getParent().runTestFunction([this](std::shared_ptr<Client> client) {return this->getTestResults(client); });
    }

    ModeCommand(ClientTestApplication& parent, const std::string& description)
      : commandline::ChildCommandOf<ClientTestApplication>(std::to_string(MODE), description, parent) {
    }
  };

  template <unsigned MODE>
  class RecordBasedModeCommand : public ModeCommand<MODE> {
  protected:
    commandline::Parameters getSupportedParameters() const override {
      return commandline::ChildCommandOf<ClientTestApplication>::getSupportedParameters()
        + commandline::Parameter("identifier", "Record identifier").value(commandline::Value<std::string>().positional().required());
    }

    std::string getRecordIdentifier() const {
      return this->getParameterValues().template get<std::string>("identifier");
    }

    RecordBasedModeCommand(ClientTestApplication& parent, const std::string& description)
      : ModeCommand<MODE>(parent, description) {
    }
  };

  class Mode1Command : public RecordBasedModeCommand<1> {
  protected:
    rxcpp::observable<bool> getTestResults(std::shared_ptr<Client> client) override;
  public:
    explicit Mode1Command(ClientTestApplication& parent) : RecordBasedModeCommand<1>(parent, "Test single storage and retrieval of data") {}
  };

  class Mode2Command : public RecordBasedModeCommand<2> {
  protected:
    rxcpp::observable<bool> getTestResults(std::shared_ptr<Client> client) override;
  public:
    explicit Mode2Command(ClientTestApplication& parent) : RecordBasedModeCommand<2>(parent, "Retrieve all data") {}
  };

  class Mode4Command : public RecordBasedModeCommand<4> {
  protected:
    rxcpp::observable<bool> getTestResults(std::shared_ptr<Client> client) override;
  public:
    explicit Mode4Command(ClientTestApplication& parent) : RecordBasedModeCommand<4>(parent, "Store 1000 records") {}
  };

  class Mode5Command : public ModeCommand<5> {
  private:
    rxcpp::observable<std::tuple<VersionResponse, std::string>> tryGetServerVersion(std::shared_ptr<const TypedClient> client, std::string name);
  protected:
    rxcpp::observable<bool> getTestResults(std::shared_ptr<Client> client) override;
  public:
    explicit Mode5Command(ClientTestApplication& parent) : ModeCommand<5>(parent, "Get version of all servers") {}
  };

  std::vector<std::shared_ptr<commandline::Command>> createChildCommands() override {
    std::vector<std::shared_ptr<commandline::Command>> result;
    result.push_back(std::make_shared<Mode1Command>(*this));
    result.push_back(std::make_shared<Mode2Command>(*this));
    result.push_back(std::make_shared<Mode4Command>(*this));
    result.push_back(std::make_shared<Mode5Command>(*this));
    return result;
  }
};

rxcpp::observable<bool> ClientTestApplication::Mode1Command::getTestResults(std::shared_ptr<Client> client) {
  std::cout << "Testing storing and retrieving of single data item" << std::endl;

  PolymorphicPseudonym pp = client->generateParticipantPolymorphicPseudonym(this->getRecordIdentifier());

  // make sure we hit the pagestore with our payload:
  auto strPayload = std::make_shared<std::string>();
  for (size_t i=0; strPayload->size() < INLINE_PAGE_THRESHOLD; i++) {
    *strPayload += " and " + std::to_string(i);
  }

  LOG(LOG_TAG, debug) << "CoreClient.StoreData";
  // Test storage and retrieval of data
  return client->storeData2(pp, "ParticipantInfo", strPayload,
                            {MetadataXEntry::MakeFileExtension(".txt")})
      .concat_map([client, pp](const DataStorageResult2& result) {
        auto id = result.mIds.at(0);
        std::cout << "Stored data with result.primaryKey: "
            << boost::algorithm::hex(id) << std::endl;

        requestTicket2Opts tOpts;
        tOpts.modes = {"read"};
        tOpts.pps = {pp.rerandomize()};
        tOpts.columns = {"ParticipantInfo"};
        return client->requestTicket2(tOpts).flat_map(
          [client, id](IndexedTicket2 ticket) {
            return client->retrieveData2(client->getMetadata({id}, ticket.getTicket()), ticket.getTicket(), true);
          });
      })
      .flat_map([](std::shared_ptr<RetrieveResult> result) {
        return result->mContent->op(RxConcatenateStrings());
      })
      .op(RxGetOne("result"))
      .map(
        [strPayload](const std::string& szResult) {
          std::cout << "Received data : " << szResult << std::endl;
          if (szResult != *strPayload) {
            std::cout << "Unexpected return data" << std::endl;
            return false;
          } else {
            std::cout << "Expected return data" << std::endl;
            return true;
          }
        });
}

rxcpp::observable<bool> ClientTestApplication::Mode2Command::getTestResults(std::shared_ptr<Client> client) {
  std::cout << "Retrieving all short pseudonyms" << std::endl;

  PolymorphicPseudonym pp = client->generateParticipantPolymorphicPseudonym(this->getRecordIdentifier());

  enumerateAndRetrieveData2Opts opts;
  opts.pps = {pp};
  opts.columnGroups = {"ShortPseudonyms"};
  return client->enumerateAndRetrieveData2(opts)
  .tap(
    [](EnumerateAndRetrieveResult result) {
      std::cout << "Primary key: " << result.mId << std::endl;
      std::cout << "Column: " << result.mColumn << std::endl;
      std::cout << "Data: " << result.mData << std::endl;
    })
    .op(RxInstead(true));
}

rxcpp::observable<bool> ClientTestApplication::Mode4Command::getTestResults(std::shared_ptr<Client> client) {
  std::cout << "Testing storing of 1000 data items" << std::endl;

  PolymorphicPseudonym pp = client->generateParticipantPolymorphicPseudonym(this->getRecordIdentifier());

  auto lpPayload = std::string("TestTest");

  // Test storage of data
  std::vector<rxcpp::observable<DataStorageResult2>> pepRequests;
  int i;
  for (i = 0; i < 10; i++) {
    pepRequests.push_back(client->storeData2(pp, "ParticipantInfo",
                std::make_shared<std::string>(lpPayload), { MetadataXEntry::MakeFileExtension(".txt") }));
    std::cout << i;
  }

  return rxcpp::observable<>::iterate(pepRequests)
  .merge() // if this is concat(), the requests are send serially
  // does not work yet until we have a better boost threading integration //.timeout(std::chrono::milliseconds(1000))
  .tap(
    [](DataStorageResult2 result) {
      std::cout << "Stored data with result.primaryKey: "
        << boost::algorithm::hex(result.mIds.at(0)) << std::endl;
    })
    .op(RxInstead(true));
}

rxcpp::observable<std::tuple<VersionResponse, std::string>> ClientTestApplication::Mode5Command::tryGetServerVersion(std::shared_ptr<const TypedClient> client, std::string name) {
  rxcpp::observable<VersionResponse> version;
  if (client == nullptr) {
    version = rxcpp::observable<>::empty<VersionResponse>();
  }
  else {
    version = client->requestVersion();
  }
  return version.zip(rxcpp::rxs::just(std::move(name)));
}

rxcpp::observable<bool> ClientTestApplication::Mode5Command::getTestResults(std::shared_ptr<Client> client) {
  std::shared_ptr<SemanticVersion> ownBinarySemver = std::make_shared<SemanticVersion>(BinaryVersion::current.getSemver());
  std::shared_ptr<SemanticVersion> ownConfigSemver{};
  auto configVersion = ConfigVersion::Current();
  if(configVersion){
    ownConfigSemver = std::make_shared<SemanticVersion>(configVersion->getSemver());
  }

  return client->getAccessManagerVersion().zip(rxcpp::rxs::just(std::string("Access Manager"))).merge(
    this->tryGetServerVersion(client->getTranscryptorClient(false), "Transcryptor"),
    this->tryGetServerVersion(client->getKeyClient(false), "Key Server"),
    this->tryGetServerVersion(client->getStorageClient(false), "Storage Facility"),
    client->getRegistrationServerVersion().zip(rxcpp::rxs::just(std::string("Registration Server"))),
    client->getAuthserverVersion().zip(rxcpp::rxs::just(std::string("Auth Server")))
  ).map([ownBinarySemver, ownConfigSemver](std::tuple<VersionResponse, std::string> response) {
    const BinaryVersion& serverBinaryVersion = std::get<0>(response).binary; 
    std::optional<ConfigVersion> serverConfigVersion = std::get<0>(response).config;

    const std::string& server = std::get<1>(response);
    std::cout << server
      << " Binary version " << serverBinaryVersion.getSummary()
      << std::endl;

    bool result = IsSemanticVersionEquivalent(*ownBinarySemver, serverBinaryVersion.getSemver());

    if (ownConfigSemver && serverConfigVersion.has_value()){
      std::cout << server
       << " Config version " << serverConfigVersion->getSummary()
       << std::endl;
      if (!IsSemanticVersionEquivalent(*ownConfigSemver, serverConfigVersion->getSemver())){
        result = false;
      }
    }
    return result;
    });
}

}

PEP_DEFINE_MAIN_FUNCTION(ClientTestApplication)
