#include <pep/authserver/OAuthProvider.hpp>

#include <sstream>

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <rxcpp/operators/rx-observe_on.hpp>

#include <pep/auth/UserGroups.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/Base64.hpp>
#include <pep/utils/Sha.hpp>
#include <pep/auth/OAuthToken.hpp>
#include <pep/utils/UriEncode.hpp>
#include <pep/utils/File.hpp>
#include <pep/async/OnAsio.hpp>
#include <pep/authserver/AuthserverBackend.hpp>
#include <pep/utils/Random.hpp>
#include <pep/utils/ChronoUtil.hpp>

#ifdef ERROR_ACCESS_DENIED
# undef ERROR_ACCESS_DENIED
#endif

#ifndef ENABLE_OAUTH_TEST_USERS
namespace {
  const std::string PRIMARY_UID_HEADER = "PEP-Primary-Uid";
  const std::string HUMAN_READABLE_UID_HEADER = "PEP-Human-Readable-Uid";
  const std::string ALTERNATIVE_UIDS_HEADER = "PEP-Alternative-Uids";
  const std::string SPOOF_CHECK_HEADER = "PEP-Spoof-Check";
}
#endif

namespace pep {

namespace ph = std::placeholders;

static const std::string LOG_TAG ("OAuthProvider");

const std::string OAuthProvider::RESPONSE_TYPE_CODE = "code";
const std::string OAuthProvider::GRANT_TYPE_AUTHORIZATION_CODE = "authorization_code";

const std::string OAuthProvider::ERROR_INVALID_REQUEST = "invalid_request";
const std::string OAuthProvider::ERROR_INVALID_CLIENT = "invalid_client";
const std::string OAuthProvider::ERROR_ACCESS_DENIED = "access_denied";
const std::string OAuthProvider::ERROR_UNAUTHORIZED_CLIENT = "unauthorized_client";
const std::string OAuthProvider::ERROR_UNSUPPORTED_RESPONSE_TYPE = "unsupported_response_type";
const std::string OAuthProvider::ERROR_UNSUPPORTED_GRANT_TYPE = "unsupported_grant_type";
const std::string OAuthProvider::ERROR_INVALID_SCOPE = "invalid_scope";
const std::string OAuthProvider::ERROR_SERVER_ERROR = "server_error";
const std::string OAuthProvider::ERROR_TEMPORARILY_UNAVAILABLE = "temporarily_unavailable";
const std::string OAuthProvider::ERROR_INVALID_GRANT = "invalid_grant";

#include <pep/authserver/GroupSelectionForm.template.inc>

namespace {

HTTPResponse MakeHttpResponse(const std::string& status, const std::string& body, std::string pageType, std::map<std::string, std::string, CaseInsensitiveCompare> headers = std::map<std::string, std::string, CaseInsensitiveCompare>()) {
  // Adds headers for all HTTP responses
  pageType.append(";charset=UTF-8");
  headers.emplace("Content-Type", pageType);
  headers.emplace("Cache-Control", "no-store");
  headers.emplace("Pragma", "no-cache");
  return HTTPResponse(status, body, headers);
}

HTTPResponse MakeErrorTextHttpResponse(const std::string& status, const std::string& body) {
  std::string beforeBody;
  if (!body.empty()) {
    beforeBody = ": \n";
  }
  LOG(LOG_TAG, error) << "Returning error HTTP response with status " << status
    << beforeBody
    << body;
  return MakeHttpResponse(status, body, "text/plain");
}

HTTPResponse MakeErrorJsonHttpResponse(const std::string& error, const std::string& description = "") {
  boost::property_tree::ptree responseData;
  responseData.add("error", error);
  if(!description.empty()) {
    responseData.add("error_description", description);
  }
  std::ostringstream oss;
  boost::property_tree::write_json(oss, responseData);
  LOG(LOG_TAG, pep::warning) << "Returning error HTTP response with status 400 Bad Request";
  std::string status = "400 Bad Request";
  if(error == OAuthProvider::ERROR_SERVER_ERROR) {
    status = "500 Internal Server Error";
  }
  return MakeHttpResponse(status, oss.str(), "application/json");
}

HTTPResponse MakeErrorRedirect(HTTPRequest::Uri redirectUri, const std::string& error, const std::string& description = "") {
  redirectUri.setQuery("error", error);
  if(!description.empty()) {
    redirectUri.setQuery("error_description", description);
  }
  LOG(LOG_TAG, pep::info) << "Returning error HTTP response with status 302 Found";
  return MakeHttpResponse("302 Found", "", "text/plain", {{"Location", redirectUri.toString()}});
}

} // End anonymous namespace

OAuthProvider::Parameters::Parameters(std::shared_ptr<boost::asio::io_service> io_service, const Configuration& config) : io_service(io_service) {
  std::optional<std::filesystem::path> spoofKeyFile;
  try {
    httpPort = config.get<uint16_t>("HTTPListenPort");
    activeGrantExpiration = std::chrono::seconds(config.get<unsigned int>("ActiveGrantExpirationSeconds"));
    spoofKeyFile = config.get<std::optional<std::filesystem::path>>("SpoofKeyFile");
    httpsCertificateFile = config.get<std::optional<std::filesystem::path>>("HTTPSCertificateFile");
  }
  catch (std::exception& e) {
    LOG(LOG_TAG, critical) << "Error with configuration file: " << e.what();
    throw;
  }
#ifndef ENABLE_OAUTH_TEST_USERS
  try {
    if (!spoofKeyFile) {
      throw std::runtime_error("Path to SpoofKeyFile not configured");
    }
    spoofKey = ReadFile(*spoofKeyFile);
    boost::trim(spoofKey);
  }
  catch (std::exception& e) {
    LOG(LOG_TAG, critical) << "Error while reading spoofkey file: " << e.what();
    throw;
  }
#endif
}

uint16_t OAuthProvider::Parameters::getHttpPort() const {
  return httpPort;
}

std::chrono::seconds OAuthProvider::Parameters::getActiveGrantExpiration() const {
  return activeGrantExpiration;
}

const std::string& OAuthProvider::Parameters::getSpoofKey() const {
  return spoofKey;
}

const std::optional<std::filesystem::path>& OAuthProvider::Parameters::getHttpsCertificateFile() const {
  return httpsCertificateFile;
}

std::shared_ptr<boost::asio::io_service> OAuthProvider::Parameters::getIOservice() const {
  return io_service;
}

void OAuthProvider::Parameters::check() const {
  if(httpPort == 0) {
    throw std::runtime_error("httpPort must be set");
  }
  if(activeGrantExpiration.count() == 0) {
    throw std::runtime_error("activeGrantExpiration must be set");
  }
  if(!io_service) {
    throw std::runtime_error("io_service must be set");
  }
  #ifndef ENABLE_OAUTH_TEST_USERS
  if(spoofKey.empty()) {
    throw std::runtime_error("spoofkey must be set");
  }
  #endif
}

OAuthProvider::OAuthProvider(const Parameters& params, std::shared_ptr<AuthserverBackend> authserverBackend)
: httpServer(std::make_shared<HTTPServer>(params.getHttpPort(), params.getIOservice(), params.getHttpsCertificateFile())),
  activeGrantExpiration(params.getActiveGrantExpiration()),
  spoofKey(params.getSpoofKey()), authserverBackend(authserverBackend),
  io_service(params.getIOservice()), mTemplates("templates") {
  httpServer->registerHandler("/auth", true, std::bind(&OAuthProvider::handleAuthorizationRequest, this, ph::_1), "");
  httpServer->registerHandler("/token", true, std::bind(&OAuthProvider::handleTokenRequest, this, ph::_1), "POST");
  httpServer->registerHandler("/code", true, std::bind(&OAuthProvider::handleCodeRequest, this, ph::_1), "");


  activeGrantsCleanupSubscription = rxcpp::rxs::interval(std::chrono::minutes(1))
              .subscribe_on(rxcpp::observe_on_new_thread()) //We want to run the interval on a different thread, otherwise it blocks the main thread
              .observe_on(observe_on_asio(*io_service)) //We want to run the cleaning up code on the io thread, so we don't have to worry about multithreading issues
              .subscribe([this](auto) {
    LOG(LOG_TAG, debug) << "Cleaning up expired grants";

    auto now = std::chrono::steady_clock::now();
    for(auto it = activeGrants.begin(); it != activeGrants.end(); /* updated inside loop */) {
      if(now - it->second.createdAt > this->activeGrantExpiration) {
        LOG(LOG_TAG, debug) << "Removed expired grant";
        it = activeGrants.erase(it);
      }
      else {
        ++it;
      }
    }
  });
}

OAuthProvider::~OAuthProvider() {
  activeGrantsCleanupSubscription.unsubscribe();
}

void OAuthProvider::addActiveGrant(const std::string& code, grant g) {
  activeGrants.emplace(code, g);
}

std::optional<OAuthProvider::grant> OAuthProvider::getActiveGrant(const std::string& code) {
  auto it = activeGrants.find(code);
  if(it == activeGrants.end()) {
    return std::nullopt;
  }
  grant g = it->second;
  activeGrants.erase(it);
  auto now = std::chrono::steady_clock::now();
  if(now - g.createdAt < activeGrantExpiration) {
    return g;
  }
  else {
    return std::nullopt;
  }
}

HTTPResponse OAuthProvider::handleAuthorizationRequest(HTTPRequest request) {
#ifdef ENABLE_OAUTH_TEST_USERS
  LOG(LOG_TAG, pep::critical) << "OAuth test users enabled. This must not happen in production!";
  if(!request.hasQuery("primary_uid") || !request.hasQuery("human_readable_uid")) {
    std::pair<std::string, std::string> testUsers[] = {
      {"assessor@master.pep.cs.ru.nl", user_group::ResearchAssessor},
      {"monitor@master.pep.cs.ru.nl", user_group::Monitor}, //e-mail addresses can contain comma's if the part before the @ is quoted
      {"dataadmin@master.pep.cs.ru.nl", user_group::DataAdministrator}, //Make sure "+" is handled correctly when URL encoding/decoding
      {"accessadmin@master.pep.cs.ru.nl", user_group::AccessAdministrator},
      {"multihat@master.pep.cs.ru.nl", "Someone with all roles"},
      {"eve@university-of-adversaries.com", "Someone without access"}
    };
    HTTPRequest::Uri linkUri = request.getUri();
    std::ostringstream body;
    body << "<html><body>";
    for(auto& [uid, description] : testUsers) {
      linkUri.setQuery("primary_uid", encodeBase64URL(uid));
      linkUri.setQuery("human_readable_uid", UriEncode(uid));
      body << "<a href=\"" << linkUri.toString() << "\">" << description << "</a><br>";
    }
    body << "</body></html>";
    return HTTPResponse("200 OK", body.str());
  }
  std::string primaryUid = request.query("primary_uid");
  std::string humanReadableUid = UriDecode(request.query("human_readable_uid"));
  std::string alternativeUidsString{};
  if (request.hasQuery("alternative_uids")) {
    alternativeUidsString = request.query("alternative_uids");
  }
#else
  if(!request.hasHeader(SPOOF_CHECK_HEADER) || request.header(SPOOF_CHECK_HEADER) != spoofKey) {
    LOG(LOG_TAG, critical) << "Spoofkey was not correctly set on the request. Looks like someone has direct access to the authserver, without being authenticated first";
    return MakeErrorTextHttpResponse("500 Internal Server Error", "Internal Server Error");
  }
  for(auto& header : {PRIMARY_UID_HEADER, HUMAN_READABLE_UID_HEADER, ALTERNATIVE_UIDS_HEADER})
    if(!request.hasHeader(header)) {
      LOG(LOG_TAG, error) << "No user header '" << header << "' received. Apache/Shibboleth is misconfigured.";
      return MakeErrorTextHttpResponse("500 Internal Server Error", "Internal Server Error");
    }
  std::string primaryUid = request.header(PRIMARY_UID_HEADER);
  std::string humanReadableUid = request.header(HUMAN_READABLE_UID_HEADER);
  std::string alternativeUidsString = request.header(ALTERNATIVE_UIDS_HEADER);
#endif //ENABLE_OAUTH_TEST_USERS
  std::vector<std::string> alternativeUids;
  boost::split(alternativeUids, alternativeUidsString, boost::is_any_of(","));
  for (auto& val : alternativeUids) { val = UriDecode(val); }
  alternativeUids.push_back(humanReadableUid);

  for(auto& q : {"client_id", "redirect_uri"}) {
    if(!request.hasQuery(q)) {
      return MakeErrorTextHttpResponse("400 Bad Request", std::string(q) + " required");
    }
  }
  const std::string& clientId = request.query("client_id");
  const std::string& redirectUriString = request.query("redirect_uri");

  std::unordered_set<std::string> registeredUris = getRegisteredRedirectURIs(clientId);
  if(registeredUris.empty()) {
    return MakeErrorTextHttpResponse("403 Forbidden", "client_id not registered");
  }
  if(registeredUris.find(redirectUriString) == registeredUris.end()) {
    return MakeErrorTextHttpResponse("403 Forbidden", "Specified redirect_uri is not registered");
  }

  HTTPRequest::Uri redirectUri(redirectUriString);
  if(request.hasQuery("state")) {
    redirectUri.setQuery("state", request.query("state"));
  }

  //We now have enough verified information to perform a redirect, so errors are from now on returned via a redirect

  try {
    for(auto& q : {"response_type", "code_challenge", "code_challenge_method"}) {
      if(!request.hasQuery(q)) {
        std::ostringstream oss;
        oss << q << " required";
        return MakeErrorRedirect(redirectUri, ERROR_INVALID_REQUEST, oss.str());
      }
    }

    const std::string& responseType = request.query("response_type");
    const std::string& codeChallenge = request.query("code_challenge");
    const std::string& codeChallengeMethod = request.query("code_challenge_method");

    if(responseType != RESPONSE_TYPE_CODE) {
      return MakeErrorRedirect(redirectUri, ERROR_UNSUPPORTED_RESPONSE_TYPE, "Only response type 'code' is supported.");
    }

    if(codeChallengeMethod != "S256") {
      return MakeErrorRedirect(redirectUri, ERROR_INVALID_REQUEST, "Only code challenge type 'S256' is supported");
    }

    std::optional<int64_t> userId = authserverBackend->findUserAndStorePrimaryIdIfMissing(primaryUid, alternativeUids);
    if(!userId){
      return MakeErrorRedirect(redirectUri, ERROR_ACCESS_DENIED, "Unknown user");
    }
    std::unordered_set<std::string> groups = authserverBackend->getUserGroups(*userId);
    if(groups.empty()) {
      return MakeErrorRedirect(redirectUri, ERROR_ACCESS_DENIED, "The user is not in any user groups");
    }

    std::string group = *groups.begin();
    if(groups.size() > 1) {
      auto formData = request.getBodyAsFormData();
      auto groupQuery = formData.find("user_group");
      if(groupQuery != formData.end()) {
        std::string selectedGroup = UriDecode(groupQuery->second);
        if(groups.find(selectedGroup) != groups.end()) {
          group = selectedGroup;
        }
        else {
          LOG(LOG_TAG, warning) << "Trying to login with group '" << selectedGroup << "', but user is not a member of that group.";
          return MakeErrorRedirect(redirectUri, ERROR_ACCESS_DENIED, "User is not a member of selected group");
        }
      }
      else {
        std::ostringstream body;
        body << BEGIN_GROUP_SELECTION_TEMPLATE;
        std::set<std::string> sortedGroups;
        std::copy(groups.cbegin(), groups.cend(), std::inserter(sortedGroups, sortedGroups.begin()));
        for(auto& g : sortedGroups) {
          body << "<option>" << g << "</option>";
        }
        body << END_GROUP_SELECTION_TEMPLATE;
        return HTTPResponse("200 OK", body.str());
      }
    }

    auto longLivedValidity = request.getQueries().find("long_lived_validity");

    std::optional<std::chrono::seconds> validityDuration;
    if(longLivedValidity != request.getQueries().end()) {
      auto maxValidity = authserverBackend->getMaxAuthValidity(group);
      if(!maxValidity) {
        return MakeErrorRedirect(redirectUri, ERROR_ACCESS_DENIED, "User is not allowed to request long-lived tokens");
      }
      if(boost::iequals(longLivedValidity->second, "max")) {
        validityDuration = maxValidity;
      }
      else {
        validityDuration = std::chrono::seconds(boost::lexical_cast<int64_t>(longLivedValidity->second));
        if(*validityDuration > *maxValidity) {
          return MakeErrorRedirect(redirectUri, ERROR_ACCESS_DENIED, "User is not allowed to request long-lived tokens for the requested duration");
        }
      }
    }

    std::string code = encodeBase64URL(RandomString(32));
    addActiveGrant(code, grant(clientId, std::move(*userId), humanReadableUid,  group, redirectUriString, codeChallenge, validityDuration));

    redirectUri.setQuery("code", code);

    return HTTPResponse("302 Found", "", {{"Location", redirectUri.toString()}});
  }
  catch (std::exception& e) {
    LOG(LOG_TAG, error) << "Unexpected error: " << e.what();
    return MakeErrorRedirect(redirectUri, ERROR_SERVER_ERROR);
  }
}

HTTPResponse OAuthProvider::handleTokenRequest(HTTPRequest request) {
  try {
    auto formData = request.getBodyAsFormData();
    for(auto& p : {"client_id", "redirect_uri", "grant_type", "code", "code_verifier"}) {
      if(formData.find(p) == formData.end()) {
        std::ostringstream oss;
        oss << p << " required";
        return MakeErrorJsonHttpResponse(ERROR_INVALID_REQUEST, oss.str());
      }
    }

    const std::string& clientId = formData.at("client_id");
    const std::string& redirectUri = formData.at("redirect_uri");
    const std::string& grantType = formData.at("grant_type");
    const std::string& code = formData.at("code");
    const std::string& codeVerifier = formData.at("code_verifier");

    if(grantType != GRANT_TYPE_AUTHORIZATION_CODE) {
      return MakeErrorJsonHttpResponse(ERROR_UNSUPPORTED_GRANT_TYPE);
    }

    auto grant = getActiveGrant(code);
    if(!grant) {
      return MakeErrorJsonHttpResponse(ERROR_INVALID_GRANT, "Code is unknown or expired");
    }
    if(grant->codeChallenge != encodeBase64URL(Sha256().digest(codeVerifier))) {
      return MakeErrorJsonHttpResponse(ERROR_INVALID_GRANT, "Code challenge failed");
    }
    if(grant->clientId != clientId) {
      return MakeErrorJsonHttpResponse(ERROR_INVALID_REQUEST, "client_id does not match the known client_id for this code");
    }
    if(grant->redirectUri != redirectUri) {
      return MakeErrorJsonHttpResponse(ERROR_INVALID_REQUEST, "redirect_uri does not match the known redirect_uri for this code");
    }

    OAuthToken token = authserverBackend->getToken(grant->internalId, grant->humanReadableId, grant->usergroup, grant->validity);

    boost::property_tree::ptree responseData;
    responseData.add("access_token", token.getSerializedForm());
    responseData.add("token_type", "bearer");
    responseData.add("expires_in", 60);
    std::ostringstream responseStream;
    boost::property_tree::write_json(responseStream, responseData);

    return MakeHttpResponse("200 OK", responseStream.str(), "application/json");
  }
  catch (const Error& err) {
    return MakeErrorJsonHttpResponse(ERROR_SERVER_ERROR, err.what());
  }
  catch(...) {
    return MakeErrorJsonHttpResponse(ERROR_SERVER_ERROR, "Internal server error");
  }
}

HTTPResponse OAuthProvider::handleCodeRequest(HTTPRequest request) {
  std::string status="200 OK";
  std::optional<std::string> error;
  std::string result;
  if (request.hasQuery("error")) {
    std::ostringstream errorstream;
    if(request.hasQuery("error_description")) {
      errorstream << request.query("error_description") + " (";
    }
    errorstream << request.query("error");
    if(request.hasQuery("error_description")) {
      errorstream << ")";
    }
    error = errorstream.str();
  }
  else if(request.hasQuery("code")) {
    std::string code = request.query("code");
    auto grantIterator = this->activeGrants.find(code);
    if(grantIterator == this->activeGrants.end()) {
      status = "404 Not Found";
      error = "Unknown code";
    }
    else {
      TemplateEnvironment::Data data {
        {"code", grantIterator->first},
        {"validity", chrono::ToString(activeGrantExpiration)}
      };

      result = mTemplates.renderTemplate("authserver/code.html.j2", data);
    }
  }
  else {
    status = "400 Bad Request";
    error =  "Did not receive an authorization code";
  }
  if(error) {
    TemplateEnvironment::Data data {
      {"error", true},
      {"content", error.value() }
    };

    result = mTemplates.renderTemplate("common/page.html.j2", data);
  }
  return MakeHttpResponse(status, result, "text/html");
}

std::unordered_set<std::string> OAuthProvider::getRegisteredRedirectURIs(const std::string& clientId) {
  //We currently only support one client_id. There are no plans to change this, so no need to make this more complicated for now.
  if(clientId == "123") {
    return { "http://127.0.0.1:16515/", "http://localhost:16515/", "/code" };
  }
  return {};
}

}
