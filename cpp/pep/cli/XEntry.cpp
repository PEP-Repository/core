#include <pep/cli/Command.hpp>
#include <pep/morphing/MorphingSerializers.hpp>

#include <boost/algorithm/hex.hpp>

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
      + pep::commandline::Parameter("encrypt", "Encrypt the payload of this entry with the file's AES key").shorthand('e')
      + pep::commandline::Parameter("bind", "Make the entry immutable by including it in the encrypted AES key's blinding").shorthand('b')
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
  }

  int execute() override {
    assert(mPayload.has_value());
    const auto& parameterValues = this->getParameterValues();

    auto name = parameterValues.get<std::string>("name");

    pep::MetadataXEntry xentry = pep::MetadataXEntry::FromStored(
      *mPayload,
      parameterValues.has("encrypt"),
      parameterValues.has("bind"));

    pep::proto::NamedMetadataXEntry nmx;

    nmx.set_name(std::move(name));
    pep::Serialization::MoveIntoProtocolBuffer(
      *nmx.mutable_value(), std::move(xentry));

    std::string result;
    if (!google::protobuf::util::MessageToJsonString(nmx, &result).ok())
      throw std::runtime_error("failed to serialize to JSON");

    std::cout << result;

    return 0;
  }
};

std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandXEntry(CliApplication& parent) {
  return std::make_shared<CommandXEntry>(parent);
}
