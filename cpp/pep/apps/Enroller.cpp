#include <pep/apps/Enroller.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/File.hpp>
#include <pep/morphing/MorphingPropertySerializers.hpp>
#include <pep/networking/EndPoint.PropertySerializer.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <cstdint>
#include <fstream>

namespace pep {

namespace {

const std::string LogTag("Enrollment");

}

EndPoint Enroller::getAccessManagerEndPoint(const Configuration& config) const {
  return config.get_child("ServerEndPoints").get<EndPoint>(ServerTraits::AccessManager().configNode());
}

rxcpp::observable<EnrolledPartyKeys> UserEnroller::enroll(std::shared_ptr<Client> client) const {
  return client->enrollUser(this->getParameterValues().get<std::string>("oauth-token"));
}

void ServiceEnroller::setProperties(Client::Builder& builder, const Configuration& config) const {
  Enroller::setProperties(builder, config);

  AsymmetricKey privateKey(ReadFile(this->getParameterValues().get<std::filesystem::path>("private-key-file")));
  X509CertificateChain certificateChain(X509CertificatesFromPem(ReadFile(this->getParameterValues().get<std::filesystem::path>("certificate-file"))));

  if (!server_.signingIdentityMatches(certificateChain)) {
    throw std::runtime_error("Cannot enroll " + server_.description() + " with certificate chain for " + certificateChain.leaf().getOrganizationalUnit().value_or("unknown party"));
  }

  builder.setSigningIdentity(std::make_shared<X509Identity>(std::move(privateKey), std::move(certificateChain)));
}

EndPoint ServiceEnroller::getAccessManagerEndPoint(const Configuration& config) const {
  if (server_ == ServerTraits::AccessManager()) {
    EndPoint result;
    result.hostname = "127.0.0.1";
    result.port = config.get<uint16_t>("ListenPort");
    result.expectedCommonName = server_.certificateSubject();
    return result;
  }

  return Enroller::getAccessManagerEndPoint(config);
}

void Enroller::setProperties(Client::Builder& builder, const Configuration& config) const {
  try {
    builder.setCaCertFilepath(config.get<std::filesystem::path>("CaCertificateFile"));

    auto serverEndPoints = config.get_child("ServerEndPoints");

    EndPoint keyServerEndPoint = serverEndPoints.get<EndPoint>(ServerTraits::KeyServer().configNode());
    builder.setKeyServerEndPoint(keyServerEndPoint);

    builder.setAccessManagerEndPoint(this->getAccessManagerEndPoint(config));

    EndPoint transcryptorEndPoint = serverEndPoints.get<EndPoint>(ServerTraits::Transcryptor().configNode());
    builder.setTranscryptorEndPoint(transcryptorEndPoint);
  }
  catch (std::exception& e) {
    throw std::runtime_error(std::string("Error with configuration file: ") + e.what());
  }
}

int Enroller::execute() {
  auto result = std::make_shared<int>(-1);

  Client::Builder builder;
  this->setProperties(builder, this->getParent().getConfiguration());

  auto io_context = std::make_shared<boost::asio::io_context>();
  builder.setIoContext(io_context);

  try {
    std::shared_ptr<Client> client = builder.build();

    this->enroll(client).subscribe([this](EnrolledPartyKeys result) {
      PEP_LOG(LogTag, Severity::Debug) << "Received EnrolledPartyKeys";

      // If output filename is provided, write output there, otherwise print it
      auto extendedProperties = this->producesExtendedProperties();
      auto values = this->getParameterValues();
      if (!extendedProperties) {
        result.signingIdentity = std::nullopt;
      }
      boost::property_tree::ptree keyConfig;
      SerializeProperties(keyConfig, result);
      if (values.has("output-path")) {
        std::ofstream output(values.get<std::filesystem::path>("output-path"));
        boost::property_tree::write_json(output, keyConfig);
      }
      else {
        boost::property_tree::write_json(std::cout, keyConfig);
        std::cout << std::endl;
      }
    }, [io_context](std::exception_ptr ep) {
      PEP_LOG(LogTag, Severity::Error) << "Exception occurred during enrollment: " << GetExceptionMessage(ep) << std::endl;
      io_context->stop();
    }, [io_context, result] {
      // Registration done
      PEP_LOG(LogTag, Severity::Info) << "Enrollment done" << std::endl;
      io_context->stop();
      *result = 0;
    });

    io_context->run();
  }
  catch (std::exception& e) {
    PEP_LOG(LogTag, Severity::Error) << e.what();
    std::cerr << e.what() << std::endl;
  }

  return *result;
}

}
