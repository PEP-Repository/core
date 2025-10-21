#include <pep/auth/UserGroup.hpp>
#include <pep/crypto/Timestamp.hpp>
#include <pep/application/CommandLineUtility.hpp>
#include <pep/auth/OAuthToken.hpp>

#include <iostream>

#include <boost/algorithm/hex.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

using namespace std::chrono;

namespace pt = boost::property_tree;

namespace {

class TokenApplication : public pep::commandline::Utility {
  std::string getDescription() const override {
    return "Creates a token for user enrollment at the key server";
  }

  std::optional<pep::severity_level> consoleLogMinimumSeverityLevel() const override {
    return std::nullopt;
  }

  pep::commandline::Parameters getSupportedParameters() const override {
    const pep::Timestamp now = pep::TimeNow();
    const auto nowSecs = pep::TicksSinceEpoch<seconds>(now);
    const auto yearLaterSecs = pep::TicksSinceEpoch<seconds>(now + years{1});

    return pep::commandline::Utility::getSupportedParameters()
      + pep::commandline::Parameter("json", "Output as json")
      + pep::commandline::Parameter("secret-json", "Loads the token secret from this file, if it exists").value(pep::commandline::Value<std::filesystem::path>().defaultsTo("OAuthTokenSecret.json"))
      + pep::commandline::Parameter("secret", "Passes the token secret directly").value(pep::commandline::Value<std::string>())
      + pep::commandline::Parameter("subject", "Specifies the \"sub\" field of the token").value(pep::commandline::Value<std::string>().defaultsTo("assessor"))
      + pep::commandline::Parameter("group", "Specifies the \"group\" field of the token").value(pep::commandline::Value<std::string>().defaultsTo(pep::UserGroup::ResearchAssessor))
      + pep::commandline::Parameter("issued-at", "Specifies the \"iat\" field of the token").value(pep::commandline::Value<seconds::rep>().defaultsTo(nowSecs, "now"))
      + pep::commandline::Parameter("expiration-time", "Specifies the \"exp\" field of the token").value(pep::commandline::Value<seconds::rep>().defaultsTo(yearLaterSecs, "a year from now"));
  }

  int execute() override {
    const auto& values = this->getParameterValues();

    // load the secret: either directly from --secret or otherwise
    // from the file specified by --secret-json.
    std::string secret;
    if (values.has("secret")) {
      secret = values.get<std::string>("secret");
    }
    else {
      // check if the file exists
      std::string fileName = values.get<std::filesystem::path>("secret-json").string(); // TODO: don't convert back and forth between boost path and string
      std::ifstream file(fileName);
      if (!file.good()) {
        // TODO: invoke this->issueCommandLineHelp instead
        throw std::runtime_error("Please specify the secret - '"
          + fileName
          + "' doesn't exist");
      }

      // load the json file into a property tree
      pt::ptree root;
      try {
        pt::read_json(fileName, root);
      }
      catch (const pt::json_parser::json_parser_error& e) {
        throw std::runtime_error("Secret file '"
          + fileName + "' is malformed: "
          + e.what());
      }

      try {
        secret = root.get<std::string>("OAuthTokenSecret");
      }
      catch (const pt::ptree_bad_path&) {
        throw std::runtime_error("Secret file '"
          + fileName + "' contains no"
          "field named 'OAuthTokenSecret'.");
      }
    }

    try {
      secret = boost::algorithm::unhex(secret);
    }
    catch (const boost::algorithm::non_hex_input&) {
      throw std::runtime_error("The secret key '"
        + secret + "' is not hex-encoded.");
    }
    catch (const boost::algorithm::not_enough_input&) {
      throw std::runtime_error("The hex - encoded secret key '"
        + secret + "' has odd length.");
    }

    if (secret.length() != 32) {
      std::cerr << "WARNING: the secret key's length is not 32"
        << " bytes, but " << secret.length() << "."
        << std::endl;
    }

    std::string subject(values.get<std::string>("subject"));
    std::string group(values.get<std::string>("group"));

    auto token = pep::OAuthToken::Generate(
      secret,
      subject,
      group,
      sys_seconds(seconds(values.get<seconds::rep>("issued-at"))),
      sys_seconds(seconds(values.get<seconds::rep>("expiration-time"))));

    // check whether the created token is valid
    if (!token.verify(secret, subject, group)) {
      std::cerr << "!! WARNING:  the generated token is not valid"
        << std::endl;
    }

    if (values.has("json"))
      token.writeJson(std::cout, false);
    else
      std::cout << token.getSerializedForm() << std::endl;

    return 0;
  }
};

}

PEP_DEFINE_MAIN_FUNCTION(TokenApplication)
