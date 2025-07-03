#include <pep/storagefacility/S3Client.hpp>
#include <pep/utils/Log.hpp>

#include <pep/utils/Sha.hpp>

#include <sstream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <rxcpp/operators/rx-concat.hpp>
#include <rxcpp/operators/rx-map.hpp>

static const std::string LOG_TAG ("S3Client");

using namespace pep;
using namespace pep::s3;

namespace
{

  // Client::Create returns an instance of the following class.
  class ClientImp
    : public  Client,
      public  HTTPSClient,
      public  std::enable_shared_from_this<ClientImp>
  {

  public:

    ClientImp(const Client::Parameters& params);

    std::shared_ptr<Client::Connection> openConnection() override;

  private:

    Credentials credentials;
    EndPoint endpoint;

    // ClientImp::openConnection returns an instance of the following class
    class Connection
      : public  Client::Connection, 
        public  HTTPSClient::Connection
        // NB. We must inherit HTTPSClient::Connection publicly here,
        // for otherwise std::make_shared<Connection>(...) doesn't set
        // the weak reference of the base class
        //
        //    std::enable_shared_from_this<TLSProtocol::Connection>,
        //
        // which causes the "shared_from_this()" to throw a bad_weak_ptr.
    {

    public:

      Connection(std::shared_ptr<ClientImp> client);

      rxcpp::observable<std::string> putObject(
          const std::string& name,
          const std::string& bucket,
          std::vector<std::shared_ptr<std::string>> payload) override;

      MessageSequence getObject(
          const std::string& name,
          const std::string& bucket) override;
      
    private:

      std::string describe() const override { 
        auto endpoint = this->getClient()->endpoint;

        return "S3 (" 
          + endpoint.hostname + ":" + std::to_string(endpoint.port)
          + (endpoint.expectedCommonName.empty() ? ""
             : " CN:" + endpoint.expectedCommonName)
          + ")";
      }

      // We gave HTTPSClient our ClientImp instance;  with this function
      // we can get it back.
      inline std::shared_ptr<ClientImp> getClient() const {
        return std::static_pointer_cast<ClientImp>(
            HTTPSClient::Connection::getClient());
      }

    };


    // helper function to create a basic unsigned S3 http request
    std::shared_ptr<HTTPRequest> request_template(
        const std::string& path,
        const std::string& method,
        std::vector<std::shared_ptr<std::string>> bodyparts={});

    // Throws an exception if the reponse doesn't have one of the accepted status codes (and logs about unexpected HTTP headers)
    void precheck_response(const HTTPResponse& resp,
      const std::vector<unsigned int>& acceptedStatusCodes);

    std::set<std::string> mUnexpectedHeaders;
  };

  // to define the constructor of ClientImp,
  // we need the following function to convert 
  // Client::Parameters to HTTPSClient::Parameters.
  using https_params_t = std::shared_ptr<HTTPSClient::Parameters>;
  https_params_t create_https_params(const Client::Parameters& params) {

    https_params_t https_params = std::make_shared<HTTPSClient::Parameters>();

    https_params->setEndPoint(params.endpoint);
    https_params->setIoContext(params.io_context);

    if (!params.ca_cert_path.empty()) {
      LOG(LOG_TAG, info) << "Using " << params.ca_cert_path.string() 
        << " to verify TLS certificate of " << params.endpoint.hostname
        << ":" << params.endpoint.port;

      https_params->setCaCertFilepath(params.ca_cert_path);
    }

    https_params->ensureContextInitialized();

    return https_params;
  }

  ClientImp::ClientImp(const Client::Parameters& params)
    : Client(),
      HTTPSClient(create_https_params(params)),
      credentials(params.credentials),
      endpoint(params.endpoint)
  {
    // nothing to do
  }


  ClientImp::Connection::Connection(std::shared_ptr<ClientImp> client)
    : Client::Connection(),
      HTTPSClient::Connection(std::static_pointer_cast<HTTPSClient>(client))
  {
  }


  std::shared_ptr<Client::Connection> ClientImp::openConnection(){
    auto result = std::make_shared<ClientImp::Connection>(shared_from_this());
    result->connect();
    return result;
  }


  std::shared_ptr<HTTPRequest> ClientImp::request_template(
      const std::string& path,
      const std::string& method,
      std::vector<std::shared_ptr<std::string>> bodyparts)
  {

    auto request = std::make_shared<HTTPRequest>(
      endpoint.hostname,
      method,
      boost::urls::url(path),
      bodyparts
    );

    return request;
  }

  void ClientImp::precheck_response(const HTTPResponse& resp,
      const std::vector<unsigned int>& acceptedStatusCodes)
  {
#ifndef SIMULATE_S3_BACKEND_FAILURE
    if (std::count(acceptedStatusCodes.begin(),
          acceptedStatusCodes.end(), resp.getStatusCode()) == 0)
#endif
    {
#ifndef SIMULATE_S3_BACKEND_FAILURE
      LOG(LOG_TAG, warning) << "HTTP Request to S3 backend gave unexpected "
        << "status line: " << resp.getStatusCode() << " "
        << resp.getStatusMessage() 
        << "; " << resp.getBody(); // TODO: remove (as it might leak info)
#else
      LOG(LOG_TAG, warning) << "Feigning failure of HTTP request"
        << " to S3 backend, because SIMULATE_S3_BACKEND_FAILURE was set.";
#endif
      throw std::runtime_error("Request to S3 backend failed.");
    }

    for (const auto& [key, value] : resp.getHeaders()) {
    
      // HTTP headers are case insensitive according to RFC2616
      static const std::set<std::string, CaseInsensitiveCompare> expected_headers = {
          "Accept-Ranges",  // we do not use this feature
          "Content-Length", // already used by the HTTPSClient class
          "Transfer-Encoding", // already used by the HTTPSClient class

          // We do not (yet) use this information, but acknowledge
          // it might be handed to us:
          "X-Amz-Bucket-Region", "Server", "Date",
          "X-Amz-Request-Id", // used for customer service
          "x-amz-storage-class", // Amazon hands us our storage class, for now we use use the STANDARD class, https://docs.aws.amazon.com/AmazonS3/latest/userguide/storage-class-intro.html
          "Content-Type",
          "Last-Modified",
          "x-amz-version-id", // see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2505
          "x-amz-checksum-crc32c", // see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2358
          // Add "x-amz-checksum-xyz" headers for other algorithms as needed: see https://docs.aws.amazon.com/AmazonS3/latest/userguide/checking-object-integrity.html

          // We're not a browser:
          "Content-Security-Policy", "X-Xss-Protection", "Vary",

          // We do use this one:
          "ETag",

          // We ignore "Connection: close", since we'll reconnect automatically.
          "Connection",

          // We don't use these from Google Cloud Storage:
          "Alt-Svc", 
          "X-GUploader-UploadID",
          "x-goog-generation",
          "x-goog-hash", // we get this via the ETag
          "x-goog-metageneration",
          "x-goog-stored-content-encoding",
          "x-goog-stored-content-length", // We use "Content-Length"
          "x-goog-storage-class",
          "Cache-Control",
          "Expires"
      };

      if (!expected_headers.contains(key)) {
        if (mUnexpectedHeaders.emplace(key).second) {
          LOG(LOG_TAG, warning) << "Unexpected header '" << key << "' in response from S3 (with value '" << value << "')";
        }
      }
    }
  }

  rxcpp::observable<std::string> ClientImp::Connection::putObject(
        const std::string& name,
        const std::string& bucket,
        std::vector<std::shared_ptr<std::string>> payload)
  {
    auto client = getClient();

    auto request = client->request_template(
        "/" + bucket + "/" + name, // path
        "PUT", payload);

    request::Sign(*request, client->credentials);

    return sendRequest(request).map(

    [client, name](HTTPResponse resp) -> std::string {

      client->precheck_response(resp, { /* acceptable status code: */ 200 });

      if (!resp.hasHeader("ETag")) {
        throw std::runtime_error("S3 did not return the MD5 hash " 
            "of the uploaded object (the 'ETag' header.)");
      }

      return resp.header("ETag");
    });

  }


  MessageSequence
    ClientImp::Connection::getObject(
        const std::string& name,
        const std::string& bucket)
  {
    auto client = getClient();

    auto request = client->request_template(
        "/" + bucket + "/" + name, 
        "GET");

    request::Sign(*request, client->credentials);

    return sendRequest(request).map(

    [client, bucket, name](HTTPResponse resp)
      -> MessageSequence {
      
      client->precheck_response(resp, { // acceptable status codes:
          200, // everything OK
          404  // it's OK if the key wasn't found
      });

      unsigned int status_code = resp.getStatusCode();

      if (status_code == 200)
        return rxcpp::observable<>::just(resp.getBodypart());

      assert(status_code == 404);

      // We must make sure we got a 404 because the key wasn't found
      // before we return no results.  If the 404 indicates for example
      // that the bucket wasn't found we want to throw an error.
      //
      // To this end we check that the error "Code" is "NoSuchKey".
      
      boost::property_tree::ptree errinf;

      {
        std::istringstream bodyss(resp.getBody());
        boost::property_tree::read_xml(bodyss, errinf);
      }

      std::string error_code = errinf.get<std::string>("Error.Code","");

      if (error_code == "") {
        throw std::runtime_error("S3 backend gave malformed error message: "
            + resp.getBody());
      }

      if (error_code != "NoSuchKey") {
        LOG(LOG_TAG, warning) << "GetObject request to S3 backend gave unexpected '"
          << error_code << "' error code, requesting '" << name << "' from bucket '"
          << bucket << "'";
        LOG(LOG_TAG, warning) << resp.getBody();

        throw std::runtime_error("Request to S3 backend failed");
      }

      return rxcpp::observable<>::empty<std::shared_ptr<std::string>>();
    
    }).concat();

  }

}

std::shared_ptr<Client> Client::Create(const Client::Parameters& p) {
  return std::make_shared<ClientImp>(p);
}
