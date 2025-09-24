#include <pep/application/Application.hpp>
#include <pep/core-client/CoreClient.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/content/ParticipantDeviceHistory.hpp>
#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>

#include <rxcpp/operators/rx-concat.hpp>
#include <rxcpp/operators/rx-filter.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-reduce.hpp>

using namespace pep::cli;

namespace {

struct ParticipantData {
  std::string id;
  std::map<std::string, std::string> deviceHistory;
};

class CommandValidate : public ChildCommandOf<CliApplication> {
public:
  explicit CommandValidate(CliApplication& parent)
    : ChildCommandOf<CliApplication>("validate", "Validate data", parent) {
  }

private:
  static std::shared_ptr<std::unordered_map<uint32_t, ParticipantData>> AddEnumerateAndRetrieveResult(std::shared_ptr<std::unordered_map<uint32_t, ParticipantData>> data, const pep::EnumerateAndRetrieveResult& result) {
    if (!result.mDataSet) {
      throw std::runtime_error("Data could not be retrieved inline"); // TODO: support this
    }
    auto& entry = (*data)[result.mLocalPseudonymsIndex];
    if (result.mColumn == "ParticipantIdentifier") {
      entry.id = result.mData;
    }
    else {
      entry.deviceHistory[result.mColumn] = result.mData;
    }
    return data;
  }

  static int ValidateData(const ParticipantData& data) {
    int result = 0;
    for (const auto& history : data.deviceHistory) {
      try {
        pep::ParticipantDeviceHistory::Parse(history.second);
      }
      catch (const std::exception& e) {
        LOG(LOG_TAG, pep::warning) << "Invalid device history in column " << history.first << " for participant " << data.id << ": " << e.what();
        result = 1;
      }
      catch (...) {
        LOG(LOG_TAG, pep::warning) << "Invalid device history in column " << history.first << " for participant " << data.id;
        result = 1;
      }
    }

    return result;
  }

  class CommandValidateData : public ChildCommandOf<CommandValidate> {
  public:
    explicit CommandValidateData(CommandValidate& parent)
      : ChildCommandOf<CommandValidate>("data", "Validate stored data", parent) {
    }

  protected:
    int execute() override {
      auto ownResult = std::make_shared<int>(0);

      auto connectivityResult = this->executeEventLoopFor(
        [ownResult](std::shared_ptr<pep::CoreClient> client) {
        return client->getGlobalConfiguration()
          .flat_map([client](std::shared_ptr<pep::GlobalConfiguration> config) {
          pep::enumerateAndRetrieveData2Opts opts;
          opts.groups = { "*" };
          opts.columns = { "ParticipantIdentifier" };
          for (const auto& device : config->getDevices()) {
            opts.columns.push_back(device.columnName);
          }
          return client->enumerateAndRetrieveData2(opts);
        })
          .reduce( // Aggregate into an unordered_map<> with an entry for each participant
            std::make_shared<std::unordered_map<uint32_t, ParticipantData>>(),
            [](std::shared_ptr<std::unordered_map<uint32_t, ParticipantData>> data, pep::EnumerateAndRetrieveResult result) { return AddEnumerateAndRetrieveResult(data, result); }
          )
          .flat_map([](std::shared_ptr<std::unordered_map<uint32_t, ParticipantData>> data) { return rxcpp::observable<>::iterate(*data); }) // Convert to an observable<key-value-pair>
          .map([](std::pair<const uint32_t, ParticipantData> pair) { return ValidateData(pair.second); }) // Validate each ParticipantData
          .filter([](int returnValue) {return returnValue != 0; }) // Keep only error return values
          .concat(rxcpp::observable<>::just(0)) // Append default return value 0 = everything OK
          .map(
            [ownResult](int returnValue) {
              if (*ownResult == 0) {
                *ownResult = returnValue;
              }
              return pep::FakeVoid();
            });
        });

      if (connectivityResult != 0) {
        return connectivityResult;
      }
      return *ownResult;
    }
  };

  class CommandValidatePseudonyms : public ChildCommandOf<CommandValidate> {
  public:
    explicit CommandValidatePseudonyms(CommandValidate& parent)
      : ChildCommandOf<CommandValidate>("pseudonym", "Validate pseudonym(s)", parent) {
    }

  protected:
    pep::commandline::Parameters getSupportedParameters() const override {
      return ChildCommandOf<CommandValidate>::getSupportedParameters()
        + pep::commandline::Parameter("pseud", "Short pseudonym or participant identifier").value(pep::commandline::Value<std::string>().positional().multiple().required());
    }

    int execute() override {
      auto args = this->getParameterValues().getMultiple<std::string>("pseud");

      auto result = EXIT_SUCCESS;
      for (const auto& pseud : args) {
        if (!pep::ShortPseudonymIsValid(pseud)) {
          std::cerr << "Pseudonym '" << pseud << "' is invalid\n";
          result = EXIT_FAILURE;
        }
      }
      return result;
    }
  };

  protected:
    std::vector<std::shared_ptr<pep::commandline::Command>> createChildCommands() override {
      std::vector<std::shared_ptr<pep::commandline::Command>> result;
      result.push_back(std::make_shared<CommandValidateData>(*this));
      result.push_back(std::make_shared<CommandValidatePseudonyms>(*this));
      return result;
    }
};

}

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandValidate(CliApplication& parent) {
  return std::make_shared<CommandValidate>(parent);
}
