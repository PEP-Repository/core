#pragma once

#include <pep/storagefacility/S3Client.hpp>
#include <pep/application/Application.hpp>
#include <pep/utils/Paths.hpp>

#include <filesystem>
#include <string>

#include <boost/lexical_cast.hpp>

namespace pep::sftest {
  // helper function
  inline std::string getenv(const char* name, std::string default_value="") {
    //NOLINTNEXTLINE(concurrency-mt-unsafe) std::getenv is thread safe as long as we do not setenv/unsetenv/putenv
    char* result = std::getenv(name);
    return result != nullptr ? std::string(result) : std::move(default_value);
  }

  // How to reach a single S3 host, and how to authenticate to it.
  // Every member has a default, so that tests can specify only the ones they care about.
  struct S3HostEnvs {
    std::string host{};
    uint16_t port{};
    std::string expectCommonName{}; // "" when not expecting a particular one
    bool useHttps{};
    s3::Credentials credentials{};
    // Certificate authority to verify the host's TLS certificate against.
    // Empty when it should be verified against the system's certificate store instead.
    std::filesystem::path caCertPath{};
  };

  // Reads the settings for a single S3 host from the "<prefix>HOST", "<prefix>PORT", ...
  // environment variables, falling back to the corresponding member of "defaults" for
  // every variable that isn't set.
  inline S3HostEnvs GetS3HostEnvs(const std::string& prefix, const S3HostEnvs& defaults) {
    return {
      .host = getenv((prefix + "HOST").c_str(), defaults.host),
      .port = boost::lexical_cast<std::uint16_t>(
            getenv((prefix + "PORT").c_str(), std::to_string(defaults.port))),
      .expectCommonName = getenv((prefix + "EXPECT_COMMON_NAME").c_str(), defaults.expectCommonName),
      .useHttps = getenv((prefix + "USE_HTTPS").c_str(), defaults.useHttps ? "1" : "0") != "0",
      .credentials = {
        .accessKey = getenv((prefix + "ACCESS_KEY").c_str(), defaults.credentials.accessKey),
        .secret = getenv((prefix + "SECRET_KEY").c_str(), defaults.credentials.secret),
        .service = getenv((prefix + "SERVICE_NAME").c_str(), defaults.credentials.service),
      },
      .caCertPath = defaults.caCertPath, // not configurable per environment variable
    };
  }

  inline std::shared_ptr<s3::Client> CreateS3Client(
      std::shared_ptr<boost::asio::io_context> io_context,
      const S3HostEnvs& hostEnvs) {

    return s3::Client::Create({
      .endpoint = EndPoint(hostEnvs.host, hostEnvs.port, hostEnvs.expectCommonName),
      .credentials = hostEnvs.credentials,
      .ioContext = std::move(io_context),
      .caCertPath = hostEnvs.caCertPath,
      .useHttps = hostEnvs.useHttps,
    });
  }

  // Environment parameters used by pepsftest
  struct Envs {
    // Both hosts serve buckets with these (same) names, so that tests can only tell the hosts
    // apart by the contents of those buckets, i.e. by the host a request was actually sent to.
    std::string s3TestBucket;
    std::string s3TestBucket2;

    std::string s3HostType; // "pep" or "external"
    // If s3HostType is "pep" then we use the following root ca cert to check
    // the tls connection.
    std::filesystem::path rootCaPath;

    // The S3 host that all S3 tests use. In the integration test setup this is the s3proxy
    // container, reached over TLS through the s3proxyproxy (nginx) container.
    S3HostEnvs hostA;
    // A second S3 host, with its own storage and its own credentials, used by tests that check
    // that pages are read from and written to the correct host. In the integration test setup
    // this is the s3proxy2 container, which is accessed over plaintext HTTP.
    S3HostEnvs hostB;

    Envs()
      : s3TestBucket(getenv("PEP_S3_TEST_BUCKET", "myBucket")),
        s3TestBucket2(getenv("PEP_S3_TEST_BUCKET2", "myBucket2")),
        s3HostType(getenv("PEP_S3_HOST_TYPE", "pep")),
        rootCaPath(GetAbsolutePath(getenv("PEP_ROOT_CA", "rootCA.cert")))
    {
      if (this->s3HostType!="pep" && this->s3HostType!="external") {
        throw std::runtime_error(
            "PEP_S3_HOST_TYPE should be either 'pep' or 'external', but is: "
            + this->s3HostType);
      }

      // Only PEP's own S3 host presents a certificate issued by our root CA. An external one
      // (e.g. Amazon's) is verified against the system's certificate store.
      const std::filesystem::path caCertPath
        = this->s3HostType=="pep" ? this->rootCaPath : std::filesystem::path();

      this->hostA = GetS3HostEnvs("PEP_S3_", {
        .host = "localhost",
        .port = 9000,
        .expectCommonName = "S3",
        .useHttps = true,
        .credentials = {.accessKey = "MyAccessKey", .secret = "MySecret"},
        .caCertPath = caCertPath,
      });
      this->hostB = GetS3HostEnvs("PEP_S3_B_", {
        .host = "localhost",
        .port = 9003,
        .useHttps = false, // no TLS terminator in front of this host: see s3proxy.sh
        .credentials = {.accessKey = "MyAccessKey2", .secret = "MySecret2"},
      });
    }
  };

}
