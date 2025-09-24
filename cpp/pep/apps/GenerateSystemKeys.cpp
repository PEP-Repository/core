#include <pep/application/Application.hpp>
#include <pep/elgamal/CurvePoint.hpp>
#include <pep/utils/Random.hpp>

#include <boost/algorithm/hex.hpp>

#include <iostream>
#include <fstream>

using namespace pep;

namespace {

const size_t HMAC_BYTES = 64;

class GenerateSystemKeysApplication : public pep::Application {
  class GenerateKeysFileCommand : public pep::commandline::ChildCommandOf<GenerateSystemKeysApplication> {
  private:
    static std::string GenerateHMACKey() {
      std::string randomBytes;
      RandomBytes(randomBytes, HMAC_BYTES);
      return boost::algorithm::hex(randomBytes);
    }

  protected:
    GenerateKeysFileCommand(const std::string& name, const std::string& description, GenerateSystemKeysApplication& parent)
      : pep::commandline::ChildCommandOf<GenerateSystemKeysApplication>(name, "Generates system keys for " + description, parent) {
    }

    pep::commandline::Parameters getSupportedParameters() const override {
      return pep::commandline::ChildCommandOf<GenerateSystemKeysApplication>::getSupportedParameters()
        + pep::commandline::Parameter("output-path", "Location of output file").value(pep::commandline::Value<std::filesystem::path>().positional().required());
    }

    virtual bool addDataBlinding() const = 0;

    int execute() override {
      GenerateKeysFile(this->getParameterValues().get<std::filesystem::path>("output-path"), this->addDataBlinding());
      return 0;
    }

  public:
    struct PrivateKeys {
      const CurveScalar pseudonyms = CurveScalar::Random();
      const CurveScalar data = CurveScalar::Random();
    };

    static PrivateKeys GenerateKeysFile(const std::filesystem::path& outPath, bool addDataBlinding) {
      PrivateKeys result;

      std::string masterPrivateKeySharePseudonyms = result.pseudonyms.text();
      std::string masterPrivateKeyShareData = result.data.text();
      std::string pseudonymsRekeyLocal = GenerateHMACKey();
      std::string pseudonymsReshuffleLocal = GenerateHMACKey();
      std::string dataRekeyLocal = GenerateHMACKey();

      std::ofstream out;
      out.open(outPath.string());
      out <<
        "{\n"
        "  \"PseudonymsRekeyLocal\": \"" << pseudonymsRekeyLocal << "\",\n"
        "  \"PseudonymsReshuffleLocal\": \"" << pseudonymsReshuffleLocal << "\",\n"
        "  \"DataRekeyLocal\": \"" << dataRekeyLocal << "\",\n";
      if (addDataBlinding) {
        std::string dataBlinding = GenerateHMACKey();
        out <<
          "  \"DataBlinding\": \"" << dataBlinding << "\",\n";
      }
      out <<
        "  \"MasterPrivateKeySharePseudonyms\": \"" << masterPrivateKeySharePseudonyms << "\",\n"
        "  \"MasterPrivateKeyShareData\": \"" << masterPrivateKeyShareData << "\"\n"
        "}";
      out.close();

      return result;
    }
  };

  template <bool ADD_DATA_BLINDING>
  class GenerateSpecificKeysFileCommand : public GenerateKeysFileCommand {
  protected:
    bool addDataBlinding() const override {
      return ADD_DATA_BLINDING;
    }

    GenerateSpecificKeysFileCommand(const std::string& name, const std::string& description, GenerateSystemKeysApplication& parent)
      : GenerateKeysFileCommand(name, description, parent) {
    }

  public:
    static PrivateKeys GenerateKeysFile(const std::filesystem::path& outPath) {
      return GenerateKeysFileCommand::GenerateKeysFile(outPath, ADD_DATA_BLINDING);
    }
  };

  class GenerateAmKeysFileCommand : public GenerateSpecificKeysFileCommand<true> {
  public:
    explicit GenerateAmKeysFileCommand(GenerateSystemKeysApplication& parent) : GenerateSpecificKeysFileCommand<true>("AM", "Access Manager", parent) {}
  };

  class GenerateTsKeysFileCommand : public GenerateSpecificKeysFileCommand<false> {
  public:
    explicit GenerateTsKeysFileCommand(GenerateSystemKeysApplication& parent) : GenerateSpecificKeysFileCommand<false>("TS", "Transcryptor", parent) {}
  };

  class GenerateBothKeysFilesCommand : public pep::commandline::ChildCommandOf<GenerateSystemKeysApplication> {
  protected:
    pep::commandline::Parameters getSupportedParameters() const override {
      return pep::commandline::ChildCommandOf<GenerateSystemKeysApplication>::getSupportedParameters()
        + pep::commandline::Parameter("am-output-path", "Location of Access Manager output file").value(pep::commandline::Value<std::filesystem::path>().positional().required())
        + pep::commandline::Parameter("ts-output-path", "Location of Transcryptor output file").value(pep::commandline::Value<std::filesystem::path>().positional().required());
    }

    int execute() override {
      auto am = GenerateAmKeysFileCommand::GenerateKeysFile(this->getParameterValues().get<std::filesystem::path>("am-output-path"));
      auto ts = GenerateTsKeysFileCommand::GenerateKeysFile(this->getParameterValues().get<std::filesystem::path>("ts-output-path"));

      CurvePoint masterPublicKeyPseudonymsPoint = CurvePoint::BaseMult(ts.pseudonyms.mult(am.pseudonyms));
      CurvePoint masterPublicKeyDataPoint = CurvePoint::BaseMult(ts.data.mult(am.data));

      std::string masterPublicKeyPseudonyms = masterPublicKeyPseudonymsPoint.text();
      std::string masterPublicKeyData = masterPublicKeyDataPoint.text();

      std::cout <<
        "PublicKeyData: " << masterPublicKeyData << std::endl <<
        "PublicKeyPseudonyms: " << masterPublicKeyPseudonyms << std::endl;

      return 0;
    }

  public:
    GenerateBothKeysFilesCommand(GenerateSystemKeysApplication& parent)
      : pep::commandline::ChildCommandOf<GenerateSystemKeysApplication>("both", "Generates system keys for both Access Manager and Transcryptor", parent) {
    }
  };

  std::vector<std::shared_ptr<pep::commandline::Command>> createChildCommands() override {
    std::vector<std::shared_ptr<pep::commandline::Command>> result;
    result.push_back(std::make_shared<GenerateAmKeysFileCommand>(*this));
    result.push_back(std::make_shared<GenerateTsKeysFileCommand>(*this));
    result.push_back(std::make_shared<GenerateBothKeysFilesCommand>(*this));
    return result;
  }

  std::string getDescription() const override {
    return "Generates the SystemKeys.json files for the access manager and/or transcryptor";
  }
};

}

PEP_DEFINE_MAIN_FUNCTION(GenerateSystemKeysApplication)
