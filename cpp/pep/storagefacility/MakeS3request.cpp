#include <pep/application/Application.hpp>
#include <pep/storagefacility/S3.hpp>
#include <boost/algorithm/string/join.hpp>

using namespace pep;

namespace {

std::string Quote(const std::string& value) {
  return '"' + value + '"';
}

}

class MakeS3request : public pep::Application {
private:

public:
  std::string getName() const override { return "MakeS3request"; }
  std::string getDescription() const override { return "Produces an S3 HTTP PUT request"; }

  commandline::Parameters getSupportedParameters() const override {
    return Application::getSupportedParameters()
      + commandline::Parameter("curl", "Output curl command instead of raw HTTP request")
      + commandline::Parameter("identity", "Identity to access S3").value(commandline::Value<std::string>().required())
      + commandline::Parameter("credential", "Credential to access S3").value(commandline::Value<std::string>().required())
      + commandline::Parameter("host", "URL of the S3 host").value(commandline::Value<std::string>().required())
      + commandline::Parameter("bucket", "Name of the bucket").value(commandline::Value<std::string>().required())
      + commandline::Parameter("object", "Name of the object").value(commandline::Value<std::string>().required())
      + commandline::Parameter("data", "Data to PUT").value(commandline::Value<std::string>().positional().required());
  }

  int execute() override {
    using namespace pep::s3;

    const auto& values = this->getParameterValues();
    boost::urls::url host(values.get<std::string>("host"));
    auto relative = values.get<std::string>("bucket") + '/' + values.get<std::string>("object");
    if (!relative.starts_with('/')) {
      relative = '/' + relative;
    }
    auto data = values.get<std::string>("data");

    HttpRequest request(host.host(), networking::HttpMethod::PUT, boost::urls::url(relative), data);
    request.completeHeaders();
    Credentials credentials{
      .accessKey = values.get<std::string>("identity"),
      .secret = values.get<std::string>("credential")
    };
    request::Sign(request, credentials);

    std::string output;
    if (values.has("curl")) {
      std::vector<std::string> parts;
      parts.emplace_back("curl -v -X PUT");
      for (const auto& header : request.getHeaders()) {
        parts.emplace_back("--header " + Quote(header.first + ": " + header.second));
      }
      parts.emplace_back("--data " + Quote(data));
      parts.emplace_back(host.c_str() + relative);
      output = boost::join(parts, " ");
    }
    else {
      output = request.toString();
    }
    std::cout << output;

    return EXIT_SUCCESS;
  }
};

PEP_DEFINE_MAIN_FUNCTION(MakeS3request)
