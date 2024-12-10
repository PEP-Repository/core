#pragma once
#include <pep/networking/HTTPSClient.hpp>
#include <pep/networking/EndPoint.hpp>
#include <pep/storagefacility/S3.hpp>

#include <rxcpp/rx-lite.hpp>

#include <filesystem>

namespace pep::s3
{
  class Client {

  public:

    // Parameters to create an Client instance, via Client::Create
    struct Parameters {

      EndPoint endpoint;  // hostname, port
      Credentials credentials; // accessKey, secret, ...

      std::shared_ptr<boost::asio::io_service> io_service;

      std::filesystem::path ca_cert_path; // ignored if not set
    };

    static std::shared_ptr<Client> Create(const Parameters& params);

    // Represents a (single socket) connection to the S3 storage provider.
    // Created using Client::openConnection();
    class Connection {
      public:

        // Adds object to given bucket, overriding any previously 
        // existing object with that name, see
        //
        //   https://docs.aws.amazon.com/AmazonS3/latest/API/RESTObjectPUT.html
        //
        // Returns the Etag (=MD5) of the payload, as computed by the S3 server.
        //
        // NB. Preventing an object from being overriden (or deleted) seems
        // possible only by an extension of S3 called "Amazon S3 Object Lock", 
        // which is not supported by Minio.
        virtual rxcpp::observable<std::string> putObject(
            const std::string& name,
            const std::string& bucket,
            std::vector<std::shared_ptr<std::string>> payload) = 0;

        inline rxcpp::observable<std::string> putObject(
            const std::string& name,
            const std::string& bucket,
            const std::string& payload) {

          return this->putObject(name, bucket, 
              std::vector<std::shared_ptr<std::string>>{
                  std::make_shared<std::string>(payload)});
        }

        // Retrieves an object from a bucket, see
        //
        //   https://docs.aws.amazon.com/AmazonS3/latest/API/RESTObjectGET.html
        //
        // The returned observable emits at most one string; no string when
        // the object wasn't found.  If no object can be returned for
        // other reasons (such as denied access) on_error is invoked.
        virtual rxcpp::observable<std::shared_ptr<std::string>> getObject(
            const std::string& name,
            const std::string& bucket) = 0;

      protected:

        Connection();
    };

    virtual std::shared_ptr<Connection> openConnection() = 0;

  protected:

    Client();
  };

}
