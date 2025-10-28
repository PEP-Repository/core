#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/application/Application.hpp>
#include <pep/async/RxBeforeCompletion.hpp>
#include <pep/async/RxGroupToVectors.hpp>
#include <pep/async/RxInstead.hpp>
#include <pep/client/Client.hpp>
#include <pep/content/Date.hpp>
#include <pep/structure/ShortPseudonyms.hpp>

#include <rxcpp/operators/rx-concat_map.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>

using namespace pep::cli;

namespace {

class CommandRegister : public ChildCommandOf<CliApplication> {
public:
  explicit CommandRegister(CliApplication& parent)
    : ChildCommandOf<CliApplication>("register", "Register participants", parent) {
  }

private:
  std::optional<int> handleVerification(const pep::ParticipantPersonalia& personalia, bool isTestParticipant, bool force) {
    if(!pep::TryParseDdMmYyyy(personalia.getDateOfBirth())) {
      throw std::runtime_error("Entered date was not valid, please use the dd-mm-yyyy format.");
    }

    //Ask the user for confirmation if --force was not specified
    if(!force) {
      char input=0;
      std::cout << "Creating participant with the following details: \n"
                << "Name: " << std::quoted(personalia.getFullName()) << "\n"
                << "Birthdate: " << std::quoted(personalia.getDateOfBirth()) << "\n"
                << "Test participant: " << (isTestParticipant ? "yes" : "no") << "\n\n"
                <<"Enter 'y' if you want to create this participant, enter any other character if you want to cancel." << std::endl;

      std::cin >> input;
      if(input != 'y') {
        std::cerr << "Cancelled creating this participant." << std::endl;
        return 1;
      }
    }
    return {};
  }

  static rxcpp::observable<pep::FakeVoid> ProcessGeneratedID(rxcpp::observable<std::string> id) {
    auto sid = std::make_shared<std::string>();
    return id.map([sid](std::string id) {
      *sid = id;
      return pep::FakeVoid();
    })
    .op(pep::RxBeforeCompletion(
      [sid]() {
        if (*sid == "") {
          throw std::runtime_error(
            "Generated duplicate participant identifier. Please try again.");
        }
        std::cout << "Generated participant with identifier: " << *sid
                  << std::endl;
      }
    ));
  }

  int storePersonalia(const pep::ParticipantPersonalia& personalia, bool isTestParticipant) {
    return this->executeEventLoopFor(
      [&personalia, isTestParticipant](std::shared_ptr<pep::Client> client) {
        return client->registerParticipant(personalia, isTestParticipant)
          .op(ProcessGeneratedID);
      });
  }

  int generateParticipantID() {
    return this->executeEventLoopFor(
    [](std::shared_ptr<pep::Client> client) {
      return client->getRegistrationServerProxy()->registerPepId()
        .op(ProcessGeneratedID);
    });
  }

  class CommandRegisterSingle : public ChildCommandOf<CommandRegister> {
  public:
    explicit inline CommandRegisterSingle(CommandRegister& parent)
      : ChildCommandOf<CommandRegister>("participant", "Create a new participant normally", parent) {
    }

  protected:
    pep::commandline::Parameters getSupportedParameters() const override {
      return ChildCommandOf<CommandRegister>::getSupportedParameters()
        + pep::commandline::Parameter("first-name", "Participant's given name").shorthand('f').value(pep::commandline::Value<std::string>().required())
        + pep::commandline::Parameter("middle-name", "Participant's middle name").shorthand('m').value(pep::commandline::Value<std::string>().defaultsTo("", "empty string"))
        + pep::commandline::Parameter("last-name", "Participant's family name").shorthand('l').value(pep::commandline::Value<std::string>().required())
        + pep::commandline::Parameter("date-of-birth", "Participant's date of birth").shorthand('d').value(pep::commandline::Value<std::string>().required())
        + pep::commandline::Parameter("test-participant", "Register as a test participant").shorthand('t')
        + pep::commandline::Parameter("force", "Skips confirmation that correct details were entered");
    }

    int execute() override {
      const auto& values = this->getParameterValues();

      auto personalia = pep::ParticipantPersonalia(
        values.get<std::string>("first-name"),
        values.get<std::string>("middle-name"),
        values.get<std::string>("last-name"),
        values.get<std::string>("date-of-birth"));

      auto isTestParticipant = values.has("test-participant");

      //Ask the user for confirmation if --force was not specified
      bool force = values.has("force");
      auto errorCode = this->getParent().handleVerification(personalia, isTestParticipant, force);
      if(errorCode) {
        return *errorCode;
      }

      return this->getParent().storePersonalia(personalia, isTestParticipant);
    }
  };

  class CommandRegisterInteractive : public ChildCommandOf<CommandRegister> {
  public:
    explicit inline CommandRegisterInteractive(CommandRegister& parent)
      : ChildCommandOf<CommandRegister>("interactive", "Create a new participant in interactive mode", parent) {
    }

  protected:
    int execute() override {
      //Get firstname
      std::string firstName;
      std::cout << "Enter the first name for the new participant: " << std::endl;
      getline(std::cin, firstName);

      //Get tussenvoegsel
      std::string middleName;
      std::cout << "Enter the middle name for the new participant or press enter to skip: " << std::endl;
      getline(std::cin, middleName);

      //Get lastname
      std::string lastName;
      std::cout << "Enter the last name for the new participant: " << std::endl;
      getline(std::cin, lastName);

      bool dateValid = false;
      std::string dateOfBirth;

      while (!dateValid) {
        //Get date of birth
        std::cout << "Enter the participants date of birth, please use the dd-mm-yyyy format: " << std::endl;
        getline(std::cin, dateOfBirth);

        dateValid = pep::TryParseDdMmYyyy(dateOfBirth).has_value();
        if (!dateValid) {
          std::cout << "Entered date was not valid, please use the dd-mm-yyyy format." << std::endl;
        }
      }

      // Get whether it's a test participant
      std::optional<bool> isTestParticipant;
      do {
        char testParticipantYn;
        std::cout << "Register as a test participant (y/n)? ";
        std::cin >> testParticipantYn;

        switch (testParticipantYn) {
        case 'y':
        case 'Y':
          isTestParticipant = true;
          break;
        case 'n':
        case 'N':
          isTestParticipant = false;
          break;
        default:
          std::cout << "\nPlease enter a 'y' or 'n' answer" << std::endl;
        }
      } while (!isTestParticipant.has_value());
      std::cout << std::endl;

      auto personalia = pep::ParticipantPersonalia(
        firstName,
        middleName,
        lastName,
        dateOfBirth);

      //Ask the user for confirmation
      auto errorCode = this->getParent().handleVerification(personalia, *isTestParticipant, false);
      if (errorCode) {
        return *errorCode;
      }

      return this->getParent().storePersonalia(personalia, *isTestParticipant);
    }
  };
  class CommandRegisterMultiple : public ChildCommandOf<CommandRegister> {
  public:
    explicit inline CommandRegisterMultiple(CommandRegister& parent)
      : ChildCommandOf<CommandRegister>("test-participants", "Create enumerated participants for testing purposes", parent) {
    }

  protected:
    pep::commandline::Parameters getSupportedParameters() const override {
      return ChildCommandOf<CommandRegister>::getSupportedParameters()
        + pep::commandline::Parameter("number", "Number of test participants to create").shorthand('n').value(pep::commandline::Value<int>().defaultsTo(600));
    }

    void finalizeParameters() override {
      ChildCommandOf<CommandRegister>::finalizeParameters();
      int number = this->getParameterValues().get<int>("number");
      if (number < 1) {
        throw std::runtime_error("Number of participants to create must be positive");
      }
    }

    int execute() override {
      int number = this->getParameterValues().get<int>("number");
      assert(number >= 1);

      return this->executeEventLoopFor([number](std::shared_ptr<pep::CoreClient> client) {
        auto gcPtr = std::make_shared<std::shared_ptr<pep::GlobalConfiguration>>();
        auto pids = std::make_shared<std::queue<std::string>>();
        return client->getGlobalConfiguration()
          .flat_map([gcPtr, number](std::shared_ptr<pep::GlobalConfiguration> gc) {
            *gcPtr = std::move(gc);
            return rxcpp::observable<>::range(1, number);
          }).concat_map([client, gcPtr, pids](int i) { // Preserve processing order so that PID queue contains correct (i.e. incremental) storage order
            std::vector<pep::StoreData2Entry> entries;
            std::ostringstream pidss;
            const auto& format = (*gcPtr)->getGeneratedParticipantIdentifierFormat();
            pidss << format.getPrefix() << std::setfill('0') << std::setw(static_cast<int>(*format.getTotalNumberOfDigits())) << i;
            auto pid = pidss.str();
            pids->push(pid);
            auto pp = std::make_shared<pep::PolymorphicPseudonym>(
              client->generateParticipantPolymorphicPseudonym(pid));
            entries.emplace_back(pp, "ParticipantIdentifier", std::make_shared<std::string>(pid), std::vector<pep::NamedMetadataXEntry>({ pep::MetadataXEntry::MakeFileExtension(".txt") }));

            // TODO   ParticipantInfo, ...
            for (auto&& sp : (*gcPtr)->getShortPseudonyms())
              entries.emplace_back(pp,
                                   sp.getColumn().getFullName(),
                                   std::make_shared<std::string>(pep::GenerateShortPseudonym(
                                     sp.getPrefix(),
                                     static_cast<int>(sp.getLength()))),
                                   std::vector<pep::NamedMetadataXEntry>({ pep::MetadataXEntry::MakeFileExtension(".txt") }));

            return client->storeData2(entries);
          }).map([pids](const pep::DataStorageResult2& res) {
            std::cout << pids->front() << std::endl;
            pids->pop();
            std::cout << std::endl;
            return pep::FakeVoid(); // rxcpp doesn't like void observables
          })
          .as_dynamic() // Reduce compiler memory usage
          .last()
          .tap([](pep::FakeVoid dummy) {
            std::cerr << std::endl;
          });
        });
    }
  };

  class CommandRegisterID : public ChildCommandOf<CommandRegister> {
  public:
    explicit inline CommandRegisterID(CommandRegister& parent)
      : ChildCommandOf<CommandRegister>("id", "Generate a participant ID", parent) {
    }

  protected:
    int execute() override {
      return this->getParent().generateParticipantID();
    }
  };

  class CommandEnsureRegistrationComplete : public ChildCommandOf<CommandRegister> {
  public:
    explicit inline CommandEnsureRegistrationComplete(CommandRegister& parent)
      : ChildCommandOf<CommandRegister>("ensure-complete", "Completes previously registered participant records", parent) {
    }

  protected:
    pep::commandline::Parameters getSupportedParameters() const override {
      return ChildCommandOf<CommandRegister>::getSupportedParameters()
        + pep::commandline::Parameter("id", "Identifier of participant record to complete").value(pep::commandline::Value<std::string>().positional());
    }

    int execute() override {
      const auto& values = this->getParameterValues();
      std::optional<std::string> id;
      if (values.has("id")) {
        id = values.get<std::string>("id");
      }

      return executeEventLoopFor([id](std::shared_ptr<pep::Client> client) {
        return client->getGlobalConfiguration() // Get global config
          .concat_map([client, id](std::shared_ptr<pep::GlobalConfiguration> globalConfig) {
          auto spCount = globalConfig->getShortPseudonyms().size();

          // TODO: also make this work for participants that don't have any ShortPseudonyms or ParticipantIdentifier (yet)
          auto opts = std::make_shared<pep::enumerateAndRetrieveData2Opts>();
          opts->columnGroups = { "ShortPseudonyms" };
          opts->columns = { "ParticipantIdentifier" };

          rxcpp::observable<std::shared_ptr<pep::enumerateAndRetrieveData2Opts>> earOpts;

          if (id.has_value()) {
            earOpts = client->parsePPorIdentity(*id)
              .map([opts](const pep::PolymorphicPseudonym& pp) {
              opts->pps = { pp };
              return opts;
                });
          }
          else {
            opts->groups = { "*" };
            earOpts = rxcpp::observable<>::just(opts);
          }

          return earOpts // Get EAR options for the participant(s) we're going to process
            .concat_map([client](std::shared_ptr<pep::enumerateAndRetrieveData2Opts> earOpts) { return client->enumerateAndRetrieveData2(*earOpts); }) // Retrieve fields for participant(s)
            .as_dynamic() // Reduce compiler memory usage
            .op(pep::RxGroupToVectors([](const pep::EnumerateAndRetrieveResult& ear) {return ear.mLocalPseudonymsIndex; })) // Group by participant
            .concat_map([](auto participants) { return rxcpp::observable<>::iterate(*participants); }) // Iterate over participants
            .map([](const std::pair<const uint32_t, std::shared_ptr<std::vector<pep::EnumerateAndRetrieveResult>>>& pair) {return pair.second; }) // Keep only (shared_ptr to) vector of fields
            .concat_map([client, id, spCount](std::shared_ptr<std::vector<pep::EnumerateAndRetrieveResult>> fields) -> rxcpp::observable<pep::FakeVoid> {
            auto idField = std::find_if(fields->cbegin(), fields->cend(), [](const pep::EnumerateAndRetrieveResult& ear) {return ear.mColumn == "ParticipantIdentifier"; });
            if (idField == fields->cend()) {
              assert(spCount >= fields->size());
              auto spsToGenerate = spCount - fields->size();
              if (id.has_value()) {
                LOG(LOG_TAG, pep::info) << "Storing participant identifier and " << spsToGenerate << " short pseudonym(s) for " << *id;
                return client->completeParticipantRegistration(*id, false);
              }

              if (spsToGenerate == 0U) {
                LOG(LOG_TAG, pep::debug) << "Encountered participant without identifier";
              }
              else {
                LOG(LOG_TAG, pep::error) << "Cannot generate " << spsToGenerate << " short pseudonym(s) for participant without identifier";
              }
              return rxcpp::observable<>::empty<pep::FakeVoid>();
            }

            // At this point we're processing a record that has a stored ParticipantIdentifier
            assert(id.value_or(idField->mData) == idField->mData); // If an ID was specified in our invocation, it should match the stored ParticipantIdentifier
            assert(spCount + 1U >= fields->size());
            auto spsToGenerate = spCount - (fields->size() - 1U);
            if (spsToGenerate > 0U) {
              LOG(LOG_TAG, pep::info) << "Storing " << spsToGenerate << " short pseudonym(s) for " << idField->mData;
              return client->completeParticipantRegistration(idField->mData, true);
            }

            return rxcpp::observable<>::empty<pep::FakeVoid>();
              });
            })
          .as_dynamic() // Reduce compiler memory usage
          .op(pep::RxInstead(pep::FakeVoid())); // Return a single FakeVoid for the entire operation
        });
    }
  };

  protected:
  std::vector<std::shared_ptr<pep::commandline::Command>> createChildCommands() override {
    std::vector<std::shared_ptr<pep::commandline::Command>> result;
    result.push_back(std::make_shared<CommandRegisterSingle>(*this));
    result.push_back(std::make_shared<CommandRegisterInteractive>(*this));
    result.push_back(std::make_shared<CommandRegisterMultiple>(*this));
    result.push_back(std::make_shared<CommandEnsureRegistrationComplete>(*this));
    result.push_back(std::make_shared<CommandRegisterID>(*this));
    return result;
  }
};

}

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandRegister(CliApplication& parent) {
  return std::make_shared<CommandRegister>(parent);
}
