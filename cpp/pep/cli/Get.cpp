#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/cli/TicketFile.hpp>

#include <pep/utils/Exceptions.hpp>
#include <pep/core-client/CoreClient.hpp>
#include <pep/async/RxInstead.hpp>
#include <pep/utils/File.hpp>
#include <pep/morphing/MorphingSerializers.hpp>

#include <boost/algorithm/hex.hpp>

#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-concat_map.hpp>
#include <rxcpp/operators/rx-reduce.hpp>

#include <fstream>

using namespace pep::cli;

namespace {

class CommandGet : public ChildCommandOf<CliApplication> {

public:
  explicit CommandGet(CliApplication& parent)
    : ChildCommandOf<CliApplication>("get", "Retrieve file (meta)data", parent) {
  }

protected:
  inline std::optional<std::string> getRelativeDocumentationUrl() const override { return "using-pepcli#get"; }

  pep::commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf<CliApplication>::getSupportedParameters()
      + pep::cli::TicketFile::GetParameters(false)
      + pep::commandline::Parameter("output-file", "Write file contents to this file. Hyphen (-) indicates stdout.").shorthand('o').value(pep::commandline::Value<std::string>())
      + pep::commandline::Parameter("metadata", "Write file metadata to this file. Hyphen (-) indicates stdout.").shorthand('m').value(pep::commandline::Value<std::string>())
      + pep::commandline::Parameter("id", "Identifier of file to retrieve").shorthand('i').value(pep::commandline::Value<std::string>().required());
  }

  int execute() override {
    const auto& values = this->getParameterValues();

    return this->executeEventLoopFor([&values](std::shared_ptr<pep::CoreClient> client) {
      return pep::cli::TicketFile::GetTicket(*client, values, std::nullopt)
        .concat_map([&values, client](pep::IndexedTicket2 ticket) {
      std::shared_ptr<std::ostream> datastream;
      std::shared_ptr<std::ostream> metadatastream;

      int stdoutcount = 0;

      if (values.has("output-file")) {
        auto path = values.get<std::string>("output-file");
        std::shared_ptr<std::ostream> stream;
        if (path == "-") {
          stdoutcount++;
          datastream = std::shared_ptr<std::ostream>(&std::cout, [](void*){});
        } else {
          datastream = std::make_shared<std::ofstream>(path, std::ios_base::out | std::ios_base::binary);
        }
      }

      if (values.has("metadata")) {
        auto path = values.get<std::string>("metadata");
        std::shared_ptr<std::ostream> stream;
        if (path == "-") {
          stdoutcount++;
          metadatastream = std::shared_ptr<std::ostream>(&std::cout, [](void*){});
        } else
          metadatastream = std::make_shared<std::ofstream>(path);
      }

      if (stdoutcount>1) {
        throw std::runtime_error("Cannot write both data and metadata to stdout.");
      }

      if (!datastream && !metadatastream) {
        throw std::runtime_error("Please set either --output-file or --metadata.");
      }

      if (datastream) {
        LOG(LOG_TAG, pep::warning) << "Data may require re-pseudonymization. Please use `pepcli pull` instead to ensure it is processed properly.";
      }

      return client->retrieveData2(
          client->getMetadata({boost::algorithm::unhex(values.get<std::string>("id"))}, ticket.getTicket()),
          ticket.getTicket(),
          datastream != nullptr // includeData
      ).flat_map([datastream,metadatastream](std::shared_ptr<pep::RetrieveResult> result) {
        if (result->mContent) {
          return result->mContent
            ->map([datastream](const std::string& chunk) {
              *datastream << chunk;
              return pep::FakeVoid();
            }).op(RxInstead(pep::FakeVoid()))
            .as_dynamic();
        }
        if (metadatastream) {
          std::string json;
          auto mdpb = pep::Serialization::ToProtocolBuffer(
              result->mMetadataDecrypted);
          if (const auto status = google::protobuf::util::MessageToJsonString(mdpb, &json); !status.ok()) {
            throw std::runtime_error("Failed to convert metadata to JSON: " + status.ToString());
          }

          *metadatastream << json;
        }
        return rxcpp::observable<>::from(pep::FakeVoid()).as_dynamic();
      });
      });
      });
  }
};

}

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandGet(CliApplication& parent) {
  return std::make_shared<CommandGet>(parent);
}
