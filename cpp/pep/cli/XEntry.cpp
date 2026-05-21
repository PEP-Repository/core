#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/morphing/MorphingSerializers.hpp>
#include <pep/serialization/Serialization.hpp>

#include <boost/algorithm/hex.hpp>

using namespace pep::cli;

namespace {

class CommandXEntry : public ChildCommandOf<CliApplication> {
private:
  std::optional<std::string> mPayload;

public:
  explicit CommandXEntry(CliApplication& parent)
    : ChildCommandOf<CliApplication>("xentry", "Convert metadata to input for pepcli store's -x flag", parent) {
  }

private:
  static std::string GetSinglePayloadSwitchMessage() {
    return "Please specify either 'payload' or 'payload-hex' but not both";
  }

protected:
  inline std::optional<std::string> getAdditionalDescription() const override {
    return GetSinglePayloadSwitchMessage();
  }

  pep::commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf<CliApplication>::getSupportedParameters()
      + pep::commandline::Parameter("name", "Name of entry").shorthand('n').value(pep::commandline::Value<std::string>().required())
      + pep::commandline::Parameter("encrypt", std::nullopt).shorthand('e')
      + pep::commandline::Parameter("bind", std::nullopt).shorthand('b')
      + pep::commandline::Parameter("payload", "Entry value").shorthand('p').value(pep::commandline::Value<std::string>())
      + pep::commandline::Parameter("payload-hex", "Entry value specified as hex-encoded string").shorthand('x').value(pep::commandline::Value<std::string>());
  }

  void finalizeParameters() override {
    ChildCommandOf<CliApplication>::finalizeParameters();

    const auto& parameterValues = this->getParameterValues();
    if (parameterValues.has("payload") == parameterValues.has("payload-hex")) {
      throw std::runtime_error(GetSinglePayloadSwitchMessage());
    }

    if (parameterValues.has("payload")) {
      mPayload = parameterValues.get<std::string>("payload");
    }
    else {
      assert(parameterValues.has("payload-hex"));
      try {
        mPayload = boost::algorithm::unhex(
          parameterValues.get<std::string>("payload-hex"));
      }
      catch (const boost::algorithm::non_hex_input&) {
        throw std::runtime_error("Switch 'payload-hex': value is not valid hexadecimally encoded data");
      }
    }

    if (parameterValues.has("encrypt")) {
      throw std::runtime_error("Encrypted metadata is currently not supported.");
    }
    if (parameterValues.has("bind")) {
      throw std::runtime_error("Bound metadata is currently not supported.");
    }
  }

  int execute() override {
    assert(mPayload.has_value());
    const auto& parameterValues = this->getParameterValues();

    auto name = parameterValues.get<std::string>("name");

    pep::MetadataXEntry xentry = pep::MetadataXEntry::FromPlaintext(
      *mPayload,
      parameterValues.has("encrypt"),
      parameterValues.has("bind"));

    pep::proto::NamedMetadataXEntry nmx;

    nmx.set_name(std::move(name));
    pep::Serialization::MoveIntoProtocolBuffer(
      *nmx.mutable_value(), std::move(xentry));

    std::string result;
    if (const auto status = google::protobuf::util::MessageToJsonString(nmx, &result); !status.ok()) {
      throw std::runtime_error("Failed to convert metadata entry to JSON: " + status.ToString());
    }

    std::cout << result;

    return 0;
  }
};

}

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandXEntry(CliApplication& parent) {
  return std::make_shared<CommandXEntry>(parent);
}
