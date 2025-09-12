#pragma once

#include <pep/crypto/X509Certificate.hpp>
#include <pep/networking/EndPoint.hpp>
#include <pep/networking/Protocol.hpp>

namespace pep::messaging {

struct BinaryProtocol {
public:
  static std::shared_ptr<networking::Protocol::ServerParameters> CreateServerParameters(boost::asio::io_context& context, uint16_t port, X509IdentityFilesConfiguration identity);
  static std::shared_ptr<networking::Protocol::ClientParameters> CreateClientParameters(boost::asio::io_context& context, EndPoint endPoint, const std::filesystem::path& caCertFilepath);
};

}
