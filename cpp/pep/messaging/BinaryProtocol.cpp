#include <pep/messaging/BinaryProtocol.hpp>
#include <pep/networking/Tcp.hpp>
#include <pep/networking/Tls.hpp>
#include <pep/utils/Log.hpp>

namespace pep::messaging {

namespace {

[[maybe_unused]] const std::string LOG_TAG = "Messaging protocol";

}

std::shared_ptr<networking::Protocol::ServerParameters> BinaryProtocol::CreateServerParameters(boost::asio::io_context& context, uint16_t port, X509IdentityFilesConfiguration identity) {
#ifdef NO_TLS
  LOG(LOG_TAG, warning) << "Exposing server on port " << port << " over an unencrypted connection";
  return std::make_shared<networking::Tcp::ServerParameters>(context, port);
#else
  return std::make_shared<networking::Tls::ServerParameters>(context, port, std::move(identity));
#endif
}

std::shared_ptr<networking::Protocol::ClientParameters> BinaryProtocol::CreateClientParameters(boost::asio::io_context& context, EndPoint endPoint, const std::filesystem::path& caCertFilepath) {
#ifdef NO_TLS
  LOG(LOG_TAG, warning) << "Connecting to " << endPoint.describe() << " over an unencrypted connection";
  return std::make_shared<networking::Tcp::ClientParameters>(context, endPoint);
#else
  auto result = std::make_shared<networking::Tls::ClientParameters>(context, std::move(endPoint));
  result->caCertFilePath(caCertFilepath);
  return result;
#endif

}

}
