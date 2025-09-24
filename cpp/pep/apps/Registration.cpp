#include <pep/application/Application.hpp>
#include <pep/client/Client.hpp>
#include <pep/utils/Exceptions.hpp>

#include <boost/asio/io_context.hpp>
#include <rxcpp/operators/rx-timeout.hpp>

using namespace pep;

namespace {

const std::string LOG_TAG ("Registration");

class RegistrationApplication : public pep::Application {
  std::shared_ptr<Client> mClient;

  std::optional<severity_level> consoleLogMinimumSeverityLevel() const override {
    // Prevent standard streams from becoming cluttered with non-output data
    return std::nullopt;
  }

  void shutdown() {
    if (mClient == nullptr) {
      throw std::runtime_error("Cannot shutdown client before it has been opened.");
    }
    mClient->shutdown().subscribe(
      [](pep::FakeVoid unused) {},
      [](std::exception_ptr ep) {
        LOG(LOG_TAG, error) << "Unexpected error shutting down: " << pep::GetExceptionMessage(ep) << std::endl;
      });
  }

  int execute() override {
    auto config = this->loadMainConfigFile();
    auto io_context = std::make_shared<boost::asio::io_context>();
    mClient = Client::OpenClient(config, io_context);

    auto returnValue = std::make_shared<int>(-1);

    ParticipantPersonalia personalia("Jan", "van", "Jansen", "1970-01-01");
    auto isTestParticipant = true; // This sure looks like test data to me
    std::string studyContext; // Unnamed (default) context: the first one listed in Global Configuration

    mClient->registerParticipant(personalia, isTestParticipant, studyContext).subscribe([](std::string id) {
      LOG(LOG_TAG, debug) << "Received participant ID" << std::endl;
      std::cout << id << std::endl;
      }, [this, returnValue](std::exception_ptr ep) {
        try {
          std::rethrow_exception(ep);
        }
        catch (const rxcpp::timeout_error&) {
          LOG(LOG_TAG, error) << "Timeout occured during storage of pseudonyms in PEP" << std::endl;
        }
        catch (const std::exception& e) {
          LOG(LOG_TAG, error) << "Exception occured: " << e.what();
        }
        shutdown();
      }, [this, returnValue] {
        // Registration done
        *returnValue = 0;
        shutdown();
      });

    // Wait until the registration is done and the client exits
    mClient->getIoContext()->run();

    return *returnValue;
  }

  std::string getDescription() const override {
    return "Register a participant";
  }

  commandline::Parameters getSupportedParameters() const override {
    return Application::getSupportedParameters()
      + MakeConfigFileParameters(".", std::nullopt, true);
  }
};

}

PEP_DEFINE_MAIN_FUNCTION(RegistrationApplication)
