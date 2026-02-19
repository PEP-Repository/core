#include <pep/key-components/EnrollmentServer.hpp>
#include <pep/key-components/KeyComponentSerializers.hpp>
#include <pep/morphing/RepoKeys.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/Log.hpp>

#include <boost/property_tree/json_parser.hpp>
#include <prometheus/summary.h>

namespace pep {

namespace {

const std::string LOG_TAG = "Enrollment server";

}


struct EnrollmentServer::Metrics : public RegisteredMetrics {
  Metrics(std::shared_ptr<prometheus::Registry> registry, const ServerTraits& serverTraits);
  prometheus::Summary& keyComponent_request_duration;
};

EnrollmentServer::Metrics::Metrics(std::shared_ptr<prometheus::Registry> registry, const ServerTraits& serverTraits) :
  RegisteredMetrics(registry),
  keyComponent_request_duration(prometheus::BuildSummary()
    .Name("pep_" + serverTraits.metricsId() + "_keyComponent_request_duration_seconds")
    .Help("Duration of a successful keyComponent request")
    .Register(*registry)
    .Add({}, prometheus::Summary::Quantiles{
      {0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001} }, std::chrono::minutes{ 5 })) {
}


EnrollmentServer::EnrollmentServer(std::shared_ptr<Parameters> parameters) :
  SigningServer(parameters),
  mPseudonymTranslator(parameters->getPseudonymTranslator()),
  mDataTranslator(parameters->getDataTranslator()),
  mMetrics(std::make_shared<EnrollmentServer::Metrics>(mRegistry, parameters->serverTraits())) {

  RegisterRequestHandlers(*this, &EnrollmentServer::handleKeyComponentRequest);
}

messaging::MessageBatches EnrollmentServer::handleKeyComponentRequest(std::shared_ptr<SignedKeyComponentRequest> signedRequest) {
  // Generate response
  auto start_time = std::chrono::steady_clock::now();
  auto response = KeyComponentResponse::HandleRequest(
    *signedRequest,
    *mPseudonymTranslator,
    *mDataTranslator,
    *this->getRootCAs());

  mMetrics->keyComponent_request_duration.Observe(std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count()); // in seconds

  //Return result
  return messaging::BatchSingleMessage(response);
}


EnrollmentServer::Parameters::Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config)
  : SigningServer::Parameters(io_context, config) {
  std::filesystem::path systemKeysFile;

  try {
    if (auto optionalSystemKeysFile = config.get<std::optional<std::filesystem::path>>("SystemKeysFile")) {
      systemKeysFile = optionalSystemKeysFile.value();
    } else {
      //Legacy version, from when we still had a (Soft)HSM. TODO: use new version in configuration for all environments, and remove legacy version.
      systemKeysFile = config.get<std::filesystem::path>("HSM.ConfigFile");
    }
  }
  catch (std::exception& e) {
    LOG(LOG_TAG, critical) << "Error with configuration file: " << e.what();
    throw;
  }

  boost::property_tree::ptree systemKeys;
  boost::property_tree::read_json(std::filesystem::canonical(systemKeysFile).string(), systemKeys);
  systemKeys = systemKeys.get_child_optional("Keys") //Old HSMKeys.json files have the keys in a Keys-object
    .get_value_or(systemKeys); //we now also allow them to be directly in the root, resulting in cleaner SystemKeys-files
  setPseudonymTranslator(std::make_shared<PseudonymTranslator>(ParsePseudonymTranslationKeys(systemKeys)));
  setDataTranslator(std::make_shared<DataTranslator>(ParseDataTranslationKeys(systemKeys)));

}

std::shared_ptr<PseudonymTranslator> EnrollmentServer::Parameters::getPseudonymTranslator() const {
  return pseudonymTranslator;
}

std::shared_ptr<DataTranslator> EnrollmentServer::Parameters::getDataTranslator() const {
  return dataTranslator;
}

void EnrollmentServer::Parameters::setPseudonymTranslator(std::shared_ptr<PseudonymTranslator> pseudonymTranslator) {
  Parameters::pseudonymTranslator = pseudonymTranslator;
}

void EnrollmentServer::Parameters::setDataTranslator(std::shared_ptr<DataTranslator> dataTranslator) {
  Parameters::dataTranslator = dataTranslator;
}

void EnrollmentServer::Parameters::check() const {
  if (!pseudonymTranslator)
    throw std::runtime_error("pseudonymTranslator must be set");
  if (!dataTranslator)
    throw std::runtime_error("dataTranslator must be set");

  SigningServer::Parameters::check();
}


}
