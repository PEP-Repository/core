#include <pep/auth/OAuthToken.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/Base64.hpp>
#include <pep/utils/Sha.hpp>

#include <filesystem>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/archive/iterators/dataflow_exception.hpp>

namespace {

const std::string OAUTH_TOKEN_JSON_KEY = "OAuthToken";

}

namespace pep {

const std::string OAuthToken::DEFAULT_JSON_FILE_NAME = "OAuthToken.json";

bool OAuthToken::verify(const std::string& secret, const std::string& requiredSubject, const std::string& requiredGroup) const {
  LOG("OAuthToken::verify", debug) << "Verifying OAuth token " << mSerialized;
  LOG("OAuthToken::verify", debug) << "encodeBase64URL(remoteJSON): " << encodeBase64URL(mData);

  auto result = true;

  // Compute the HMAC on the json using the shared secret
  std::string localHMAC = Sha256::HMac(secret, mData);

  // Check whether the received HMAC is equal to the computed one
  if(localHMAC != mHmac) {
    LOG("OAuthToken::verify", info) << "MAC in token invalid";
    result = false;
  }

  // Verify whether the user and group in the ticket are the same as the one provided
  if (!verifySubject(requiredSubject)) {
    result = false;
  }
  if (!verifyGroup(requiredGroup)) {
    result = false;
  }

  if (!verifyValidityPeriod()) {
    result = false;
  }

  return result;
}

bool OAuthToken::verify(const std::optional<std::string>& requiredSubject, const std::optional<std::string>& requiredGroup) const {
  auto result = true;

  if (requiredSubject && !verifySubject(*requiredSubject)) {
    result = false;
  }

  if (requiredGroup && !verifyGroup(*requiredGroup)) {
    result = false;
  }

  if (!verifyValidityPeriod()) {
    result = false;
  }

  return result;
}

bool OAuthToken::verifySubject(const std::string& required) const {
  if (required != mSubject) {
    LOG("OAuthToken::verify", info) << "Subject in token '" << mSubject << "' does not match required subject '" << required << "'";
    return false;
  }
  return true;
}

bool OAuthToken::verifyGroup(const std::string& required) const {
  if (required != mGroup) {
    LOG("OAuthToken::verify", info) << "Group in token '" << mGroup << "' does not match required group '" << required << "'";
    return false;
  }
  return true;
}

bool OAuthToken::verifyValidityPeriod() const {
  //TODO does this work, or will it break with different timezones?
  std::time_t currentTime = std::time(nullptr);

  // Check time of issuance
  if (mIssuedAt >= currentTime + 60) { // Account for clock drift.  See #677
    LOG("OAuthToken::verifyValidityPeriod", info) << "Token issued after current time";
    return false;
  }

  // Check whether token already expired
  if (mExpiresAt <= currentTime) {
    LOG("OAuthToken::verifyValidityPeriod", info) << "Token expired";
    return false;
  }

  return true;
}

OAuthToken OAuthToken::Generate(
    const std::string& secret,
    const std::string& subject,
    const std::string& group,
    time_t issuedAt,
    time_t expirationTime) {

    // create the token
    std::ostringstream stream;

    // the payload json files comes first..
    boost::property_tree::ptree root;

    root.put("sub", subject);
    root.put("group", group);
    root.put("iat", issuedAt);
    root.put("exp", expirationTime);

    boost::property_tree::write_json(stream, root);
    std::string payload(stream.str());

    // or actually, in base64 encoded form..
    stream.str("");
    stream << encodeBase64URL(payload);

    // then a dot (".")..
    stream << ".";

    // and finally the (base64 encoded) hmac of the payload
    stream << encodeBase64URL(
             Sha256::HMac(secret, payload));

    return OAuthToken(stream.str());
}

OAuthToken::OAuthToken(const std::string& serialized)
  : mSerialized(serialized) {
  std::vector<std::string> splitToken;
  boost::split(splitToken, serialized, boost::is_any_of("."));

  // Token should consist of two parts: (encoded) JSON data and HMAC
  if (splitToken.size() != 2) {
    LOG("OAuthToken",info) << "Invalid token format: did not match the correct format.";
    throw std::runtime_error("Invalid token format.");
  }

  // If the HMAC part of the token is not 43 characters long, it is not a valid OAuthToken.
  if (splitToken[1].length() != 43) {
    LOG("OAuthToken",info) << "Invalid token format: HMAC was not of the correct length.";
    throw std::runtime_error("Invalid token format.");
  }

  try {
    mData = decodeBase64URL(splitToken[0]);
    mHmac = decodeBase64URL(splitToken[1]);

    boost::property_tree::ptree root;
    std::stringstream jsonStream(mData);
    boost::property_tree::read_json(jsonStream, root);

    mSubject = root.get<std::string>("sub");
    mGroup = root.get<std::string>("group");
    mIssuedAt = root.get<std::time_t>("iat");
    mExpiresAt = root.get<std::time_t>("exp");

    /* Legacy: the "exp" field was filled with milliseconds-since-epoch under some circumstances.
     * For affected tokens, we convert the faulty value to the intended seconds-since-epoch.
     * See https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2649#note_50424
     */
    if (mIssuedAt   <  2'000'000'000    // Issued by a code base affected by the bug
      && mExpiresAt > 10'000'000'000) { // Issued for a period longer than was (likely) intended
      mExpiresAt /= 1000;
    }
  }
  catch(const boost::archive::iterators::dataflow_exception& e) {
    LOG("OAuthToken",info) << "Error decoding token: " << e.what();
    throw std::runtime_error("Invalid token format.");
  }
  catch (const boost::property_tree::json_parser_error& e) {
    LOG("OAuthToken",info) << "Error parsing JSON: " << e.what();
    throw std::runtime_error("Invalid token format.");
  }
}

OAuthToken OAuthToken::ReadJson(std::istream& source) {
  boost::property_tree::ptree root;
  boost::property_tree::read_json(source, root);
  auto serialized = root.get<std::string>(OAUTH_TOKEN_JSON_KEY);
  return Parse(serialized);
}

OAuthToken OAuthToken::ReadJson(const std::filesystem::path& file) {
  const auto canonical = std::filesystem::canonical(file).string();
  std::ifstream fs(canonical, std::ios::in | std::ios::binary);
  return ReadJson(fs);
}

void OAuthToken::writeJson(std::ostream& destination, bool pretty) const {
  boost::property_tree::ptree root;
  root.put(OAUTH_TOKEN_JSON_KEY, this->getSerializedForm());
  boost::property_tree::write_json(destination, root, pretty);
}

void OAuthToken::writeJson(const std::filesystem::path& file, bool pretty) const {
  std::ofstream fs(file.string(), std::ios::out | std::ios::binary | std::ios::trunc);
  this->writeJson(fs, pretty);
}

}
