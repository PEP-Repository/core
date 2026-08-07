#pragma once

#include <pep/storagefacility/S3Client.hpp>
#include <pep/application/Application.hpp>
#include <pep/utils/Paths.hpp>

#include <filesystem>
#include <string>

namespace pep::sftest {
  // helper function
  inline std::string getenv(const char* name, std::string default_value="") {
    //NOLINTNEXTLINE(concurrency-mt-unsafe) std::getenv is thread safe as long as we do not setenv/unsetenv/putenv
    char* result = std::getenv(name);
    return result != nullptr ? std::string(result) : default_value;
  }

  // Environment parameters used by pepsftest
  struct Envs {
    static std::string portDefault;

    std::string host;
    uint16_t port;
    std::string expectCommonName;  // "" when not expecting a particular one
    std::string s3AccessKey;
    std::string s3SecretKey;
    std::string s3ServiceName;
    std::string s3TestBucket;
    std::string s3TestBucket2;

    std::string s3_host_type; // "pep" or "external"
    // If s3_host_type is "pep" then we use the following root ca cert to check
    // the tls connection.
    std::filesystem::path root_ca_path;

    Envs()
      : host(getenv("PEP_S3_HOST", "localhost")),
        port(static_cast<uint16_t>(std::stoi(getenv("PEP_S3_PORT", portDefault)))),
        expectCommonName(getenv("PEP_S3_EXPECT_COMMON_NAME", "S3")),
        s3AccessKey(getenv("PEP_S3_ACCESS_KEY", "MyAccessKey")),
        s3SecretKey(getenv("PEP_S3_SECRET_KEY", "MySecret")),
        s3ServiceName(getenv("PEP_S3_SERVICE_NAME", "s3")),
        s3TestBucket(getenv("PEP_S3_TEST_BUCKET", "myBucket")),
        s3TestBucket2(getenv("PEP_S3_TEST_BUCKET2", "myBucket2")),
        s3_host_type(getenv("PEP_S3_HOST_TYPE", "pep")),
        root_ca_path(GetAbsolutePath(
              getenv("PEP_ROOT_CA", "rootCA.cert")))
    {
      if (this->s3_host_type!="pep" && this->s3_host_type!="external") {
        throw std::runtime_error(
            "PEP_S3_HOST_TYPE should be either 'pep' or 'external', but is: "
            + this->s3_host_type);
      }
    }

    // to be passed to TLSClientParameters
    inline std::filesystem::path GetCaCertFilepath() {
      if (this->s3_host_type=="pep")
        return this->root_ca_path;
      // else
      return "";
    }

    inline std::shared_ptr<s3::Client> CreateS3Client(
        std::shared_ptr<boost::asio::io_context> io_context) {

      s3::Client::Parameters params = {{
          this->host, this->port, this->expectCommonName,
        }, {
          this->s3AccessKey,
          this->s3SecretKey,
          this->s3ServiceName,
        },
        io_context,
        this->GetCaCertFilepath(),
        true  // use_https
      };

      return s3::Client::Create(params);
    }

  };

}
