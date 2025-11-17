#include <pep/apps/Enroller.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/File.hpp>
#include <pep/networking/EndPoint.PropertySerializer.hpp>

#include <boost/asio/io_context.hpp>
#include <cstdint>
#include <fstream>

namespace pep {

namespace {

const std::string LOG_TAG("Enrollment");

}

EndPoint Enroller::getAccessManagerEndPoint(const Configuration& config) const {
  return config.get<EndPoint>("AccessManager");
}

rxcpp::observable<EnrollmentResult> UserEnroller::enroll(std::shared_ptr<Client> client) const {
  return client->enrollUser(this->getParameterValues().get<std::string>("oauth-token"));
}

void ServiceEnroller::setProperties(Client::Builder& builder, const Configuration& config) const {
  Enroller::setProperties(builder, config);

  AsymmetricKey privateKey(ReadFile(this->getParameterValues().get<std::filesystem::path>("private-key-file")));
  X509CertificateChain certificateChain(ReadFile(this->getParameterValues().get<std::filesystem::path>("certificate-file")));

  if (!mServer.signingIdentityMatches(certificateChain)) {
    std::string description = "unknown facility";
    if (!certificateChain.empty()) {
      auto certificate = certificateChain.front();
      if (auto ou = certificate.getOrganizationalUnit()) {
        description = *ou;
      }
    }
    throw std::runtime_error("Cannot enroll " + mServer.description() + " with certificate chain for " + description);
  }

  builder.setSigningIdentity(std::make_shared<X509Identity>(std::move(privateKey), std::move(certificateChain)));
}

EndPoint ServiceEnroller::getAccessManagerEndPoint(const Configuration& config) const {
  if (mServer == ServerTraits::AccessManager()) {
    EndPoint result;
    result.hostname = "127.0.0.1";
    result.port = config.get<uint16_t>("ListenPort");
    result.expectedCommonName = ServerTraits::AccessManager().tlsCertificateSubject();
    return result;
  }

  return Enroller::getAccessManagerEndPoint(config);
}

void Enroller::setProperties(Client::Builder& builder, const Configuration& config) const {
  try {
    builder.setCaCertFilepath(config.get<std::filesystem::path>("CACertificateFile"));

    EndPoint keyServerEndPoint = config.get<EndPoint>("KeyServer");
    builder.setKeyServerEndPoint(keyServerEndPoint);

    builder.setAccessManagerEndPoint(this->getAccessManagerEndPoint(config));

    EndPoint transcryptorEndPoint = config.get<EndPoint>("Transcryptor");
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

    this->enroll(client).subscribe([this](EnrollmentResult result) {
      LOG(LOG_TAG, debug) << "Received EnrollmentResult";

      // If output filename is provided, write output there, otherwise print it
      auto extendedProperties = this->producesExtendedProperties();
      auto values = this->getParameterValues();
      if (values.has("output-path")) {
        std::ofstream output(values.get<std::filesystem::path>("output-path").string());
        result.writeJsonTo(output, this->producesDataKey(), extendedProperties, extendedProperties);
      }
      else {
        result.writeJsonTo(std::cout, this->producesDataKey(), extendedProperties, extendedProperties);
        std::cout << std::endl;
      }
    }, [io_context](std::exception_ptr ep) {
      LOG(LOG_TAG, error) << "Exception occured during enrollment: " << GetExceptionMessage(ep) << std::endl;
      io_context->stop();
    }, [io_context, result] {
      // Registration done
      LOG(LOG_TAG, info) << "Enrollment done" << std::endl;
      io_context->stop();
      *result = 0;
    });

    io_context->run();
  }
  catch (std::exception& e) {
    LOG(LOG_TAG, error) << e.what();
    std::cerr << e.what() << std::endl;
  }

  return *result;
}

}
