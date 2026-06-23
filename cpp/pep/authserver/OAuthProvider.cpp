#include <pep/authserver/OAuthProvider.hpp>

#include <algorithm>
#include <sstream>

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/url/pct_string_view.hpp>
#include <boost/url/url_view.hpp>

#include <rxcpp/operators/rx-observe_on.hpp>
#include <rxcpp/operators/rx-on_error_resume_next.hpp>

#include <pep/auth/OAuthError.hpp>
#include <pep/auth/UserGroup.hpp>
#include <pep/crypto/ConstTime.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/MiscUtil.hpp>
#include <pep/utils/Base64.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/OpenSSLHasher.hpp>
#include <pep/auth/OAuthToken.hpp>
#include <pep/utils/File.hpp>
#include <pep/async/OnAsio.hpp>
#include <pep/authserver/AuthserverBackend.hpp>
#include <pep/utils/Random.hpp>
#include <pep/utils/ChronoUtil.hpp>

using namespace std::literals;
using boost::urls::url;

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

using boost::urls::url;

namespace pep {

namespace {

const std::string LogTag("OAuthProvider");

}

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

const std::string PepClientId = "123";

const std::string SERVER_ERROR_DESCRIPTION = "Internal server error";

const std::initializer_list<url> DefaultRedirectUris{
  url("http://localhost:16515/"),
  // Relative redirect URIs are not actually compliant with RFC6749,
  // see https://datatracker.ietf.org/doc/html/rfc6749#section-3.1.2,
  // but we interpret it as relative to the authserver domain
  url("/code"),
};

/// Validate provided redirect_uri according to https://datatracker.ietf.org/doc/html/rfc6749#section-3.1.2.3.
///
/// We do not allow varying the query part, see https://datatracker.ietf.org/doc/html/rfc6749#section-3.1.2.2
/// and https://www.oauth.com/oauth2-servers/redirect-uris/redirect-uri-registration/#per-request.
bool CompareRedirectUris(boost::urls::url_view providedRedirectUri, boost::urls::url_view registeredRedirectUri) {
  // Do simple string comparison
  // See also https://www.boost.org/doc/libs/1_87_0/doc/antora/url/urls/normalization.html
  return registeredRedirectUri.buffer() == providedRedirectUri.buffer();
}

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
  PEP_LOG(LogTag, Severity::Error) << "Returning error HTTP response with status " << status
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
  PEP_LOG(LogTag, pep::Severity::Warning) << "Returning error HTTP response with status 400 Bad Request";
  std::string status = "400 Bad Request";
  if(error == OAuthProvider::ERROR_SERVER_ERROR) {
    status = "500 Internal Server Error";
  }
  return MakeHttpResponse(status, std::move(oss).str(), "application/json");
}

HTTPResponse MakeErrorRedirect(url redirectUri, const std::string& error, const std::string& description) {
  assert(!error.empty());
  redirectUri.params().set("error", error);
  assert(!description.empty());
  redirectUri.params().set("error_description", description);
  PEP_LOG(LogTag, pep::Severity::Info) << "Returning error HTTP response with status 302 Found";
  return MakeHttpResponse("302 Found", "", "text/plain", {{"Location", std::string(redirectUri.buffer())}});
}

} // End anonymous namespace

OAuthProvider::Parameters::Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config)
  : ioContext_(io_context) {
  [[maybe_unused]] std::optional<std::filesystem::path> spoofKeyFile;
  try {
    httpPort_ = config.get<uint16_t>("HttpListenPort");
    activeGrantExpiration_ = std::chrono::seconds(config.get<unsigned int>("ActiveGrantExpirationSeconds"));
    spoofKeyFile = config.get<std::optional<std::filesystem::path>>("SpoofKeyFile");
    httpsCertificateFile_ = config.get<std::optional<std::filesystem::path>>("HttpsCertificateFile");
    extraRedirectUris_ = RangeToVector(
      config.get<std::optional<std::vector<std::string>>>("ExtraRedirectUris")
        .value_or(std::vector<std::string>{})
      | std::views::transform([](std::string_view str) { return url{str}; }));
  }
  catch (std::exception& e) {
    PEP_LOG(LogTag, Severity::Critical) << "Error with configuration file: " << e.what();
    throw;
  }
#ifndef ENABLE_OAUTH_TEST_USERS
  try {
    if (!spoofKeyFile) {
      throw std::runtime_error("Path to SpoofKeyFile not configured");
    }
    spoofKey_ = ReadFile(*spoofKeyFile);
    boost::trim(spoofKey_);
  }
  catch (std::exception& e) {
    PEP_LOG(LogTag, Severity::Critical) << "Error while reading spoofkey file: " << e.what();
    throw;
  }
#endif
}

uint16_t OAuthProvider::Parameters::getHttpPort() const {
  return httpPort_;
}

std::chrono::seconds OAuthProvider::Parameters::getActiveGrantExpiration() const {
  return activeGrantExpiration_;
}

const std::string& OAuthProvider::Parameters::getSpoofKey() const {
  return spoofKey_;
}

const std::optional<std::filesystem::path>& OAuthProvider::Parameters::getHttpsCertificateFile() const {
  return httpsCertificateFile_;
}

std::shared_ptr<boost::asio::io_context> OAuthProvider::Parameters::getIoContext() const {
  return ioContext_;
}

void OAuthProvider::Parameters::check() const {
  if(httpPort_ == 0) {
    throw std::runtime_error("httpPort must be set");
  }
  if(activeGrantExpiration_ == decltype(activeGrantExpiration_)::zero()) {
    throw std::runtime_error("activeGrantExpiration must be set");
  }
  if(!ioContext_) {
    throw std::runtime_error("io_context must be set");
  }
  #ifndef ENABLE_OAUTH_TEST_USERS
  if(spoofKey_.empty()) {
    throw std::runtime_error("spoofkey must be set");
  }
  #endif
}

OAuthProvider::OAuthProvider(const Parameters& params, std::shared_ptr<AuthserverBackend> authserverBackend)
: httpServer_(std::make_shared<HTTPServer>(params.getHttpPort(), params.getIoContext(), params.getHttpsCertificateFile())),
  activeGrantExpiration_(params.getActiveGrantExpiration()),
  spoofKey_(params.getSpoofKey()),
  authserverBackend_(authserverBackend),
  ioContext_(params.getIoContext()), templates_(std::filesystem::canonical(GetExecutablePath()).parent_path() / "templates") {
  httpServer_->registerHandler("/auth", true, std::bind_front(&OAuthProvider::handleAuthorizationRequest, this), "");
  httpServer_->registerHandler("/token", true, std::bind_front(&OAuthProvider::handleTokenRequest, this), "POST");
  httpServer_->registerHandler("/code", true, std::bind_front(&OAuthProvider::handleCodeRequest, this), "");

  allowedRedirectUris_.reserve(DefaultRedirectUris.size() + params.getExtraRedirectUris().size());
  std::ranges::copy(DefaultRedirectUris, std::back_inserter(allowedRedirectUris_));
  std::ranges::copy(params.getExtraRedirectUris(), std::back_inserter(allowedRedirectUris_));

  activeGrantsCleanupSubscription_ = rxcpp::rxs::interval(std::chrono::minutes(1))
              .subscribe_on(rxcpp::observe_on_new_thread()) //We want to run the interval on a different thread, otherwise it blocks the main thread
              .observe_on(ObserveOnAsio(*ioContext_)) //We want to run the cleaning up code on the io thread, so we don't have to worry about multithreading issues
              .subscribe([this](auto) {
    PEP_LOG(LogTag, Severity::Debug) << "Cleaning up expired grants";

    auto now = std::chrono::steady_clock::now();
    for(auto it = activeGrants_.begin(); it != activeGrants_.end(); /* updated inside loop */) {
      if(now - it->second.createdAt > activeGrantExpiration_) {
        PEP_LOG(LogTag, Severity::Debug) << "Removed expired grant";
        it = activeGrants_.erase(it);
      }
      else {
        ++it;
      }
    }
  });
}

OAuthProvider::~OAuthProvider() {
  activeGrantsCleanupSubscription_.unsubscribe();
}

void OAuthProvider::addActiveGrant(const std::string& code, Grant g) {
  activeGrants_.emplace(code, g);
}

std::optional<OAuthProvider::Grant>
OAuthProvider::getActiveGrant(const std::string& code) {
  auto it = activeGrants_.find(code);
  if (it == activeGrants_.end()) {
    return std::nullopt;
  }
  Grant g = it->second;
  activeGrants_.erase(it);
  auto now = std::chrono::steady_clock::now();
  if (now - g.createdAt < activeGrantExpiration_) {
    return g;
  }
  else {
    return std::nullopt;
  }
}

rxcpp::observable<HTTPResponse> OAuthProvider::handleAuthorizationRequest(HTTPRequest request, std::string remoteIp) {
  const auto& params = request.uri().params();

#ifdef ENABLE_OAUTH_TEST_USERS
  PEP_LOG(LogTag, pep::Severity::Critical) << "OAuth test users enabled. This must not happen in production!";

  auto primaryUidIt = params.find("primary_uid"),
      humanReadableUidIt = params.find("human_readable_uid");
  if(primaryUidIt == params.end() || humanReadableUidIt == params.end()) {
    std::array<std::pair<std::string, std::string>, 7> testUsers{{
      {"assessor@main.pep.cs.ru.nl", UserGroup::ResearchAssessor},
      {"monitor@main.pep.cs.ru.nl", UserGroup::Monitor},
      {"dataadmin@main.pep.cs.ru.nl", UserGroup::DataAdministrator},
      {"accessadmin@main.pep.cs.ru.nl", UserGroup::AccessAdministrator},
      {"systemadmin@main.pep.cs.ru.nl", UserGroup::SystemAdministrator},
      {"multihat@main.pep.cs.ru.nl", "Someone with all roles"},
      {"eve@university-of-adversaries.com", "Someone without access"}
    }};
    auto linkUri = request.uri();
    std::ostringstream body;
    body << "<html><body>";
    for(auto& [uid, description] : testUsers) {
      linkUri.params().set("primary_uid", EncodeBase64Url(uid));
      linkUri.params().set("human_readable_uid", uid);
      body << "<a href=\"" << linkUri << "\">" << description << "</a><br>";
    }
    body << "</body></html>";
    return rxcpp::rxs::just(HTTPResponse("200 OK", std::move(body).str()));
  }
  const std::string& primaryUid = (*primaryUidIt).value;
  const std::string& humanReadableUid = (*humanReadableUidIt).value;
  std::string alternativeUidsString{};
  if (auto it = params.find("alternative_uids"); it != params.end()) {
    alternativeUidsString = (*it).value;
  }
#else
  if(!request.hasHeader(SPOOF_CHECK_HEADER) || !const_time::IsEqual(request.header(SPOOF_CHECK_HEADER), spoofKey_)) {
    PEP_LOG(LogTag, Severity::Critical) << "Spoofkey was not correctly set on the request. Looks like someone has direct access to the authserver, without being authenticated first. Remote IP: " << remoteIp;
    return rxcpp::rxs::just(MakeErrorTextHttpResponse("500 Internal Server Error", "Internal Server Error"));
  }
  for(auto& header : {PRIMARY_UID_HEADER, HUMAN_READABLE_UID_HEADER, ALTERNATIVE_UIDS_HEADER})
    if(!request.hasHeader(header)) {
      PEP_LOG(LogTag, Severity::Error) << "No user header '" << header << "' received. Apache/Shibboleth is misconfigured.";
      return rxcpp::rxs::just(MakeErrorTextHttpResponse("500 Internal Server Error", "Internal Server Error"));
    }

  std::string primaryUid = request.header(PRIMARY_UID_HEADER);
  if (primaryUid.empty()) {
    PEP_LOG(LogTag, Severity::Error) << "Empty user header '" << PRIMARY_UID_HEADER << "' received. Apache/Shibboleth is misconfigured.";
    return rxcpp::rxs::just(MakeErrorTextHttpResponse("500 Internal Server Error", "Internal Server Error"));
  }

  std::string humanReadableUid = request.header(HUMAN_READABLE_UID_HEADER);
  if (humanReadableUid.empty()) {
    PEP_LOG(LogTag, Severity::Error) << "Empty user header '" << HUMAN_READABLE_UID_HEADER << "' received. Apache/Shibboleth is misconfigured.";
    return rxcpp::rxs::just(MakeErrorTextHttpResponse("500 Internal Server Error", "Internal Server Error"));
  }

  std::string alternativeUidsString = request.header(ALTERNATIVE_UIDS_HEADER);
#endif //ENABLE_OAUTH_TEST_USERS

  std::vector<std::string> alternativeUids;
  boost::split(alternativeUids, alternativeUidsString, std::bind_front(std::equal_to{}, ','));
  // Decode (double-)encoded list values
  for (auto& val : alternativeUids) { val = boost::urls::pct_string_view(val).decode(); }
  alternativeUids.push_back(humanReadableUid);

  auto clientIdIt = params.find("client_id"),
      redirectUriIt = params.find("redirect_uri");
  for (auto it : {clientIdIt, redirectUriIt}) {
    if (it == params.end()) {
      return rxcpp::rxs::just(MakeErrorTextHttpResponse("400 Bad Request", "client_id & redirect_uri required"));
    }
  }
  const std::string& clientId = (*clientIdIt).value;
  const std::string& redirectUriString = (*redirectUriIt).value;

  const auto& registeredUris = getRegisteredRedirectURIs(clientId);
  if(registeredUris.empty()) {
    return rxcpp::rxs::just(MakeErrorTextHttpResponse("403 Forbidden", "client_id not registered"));
  }
  if (std::ranges::find_if(registeredUris, std::bind_front(CompareRedirectUris, redirectUriString)) == registeredUris.end()) {
    return rxcpp::rxs::just(MakeErrorTextHttpResponse("403 Forbidden", "Specified redirect_uri is not registered"));
  }

  url redirectUri(redirectUriString);
  if (auto it = params.find("state"); it != params.end()) {
    redirectUri.params().set("state", (*it).value);
  }

  std::optional<std::string> longLivedValidityStr;
  //We now have enough verified information to perform a redirect, so errors are from now on returned via a redirect
  std::string codeChallenge;
  try {
    auto responseTypeIt = params.find("response_type"),
        codeChallengeIt = params.find("code_challenge"),
        codeChallengeMethodIt = params.find("code_challenge_method");
    for (auto it : {responseTypeIt, codeChallengeIt, codeChallengeMethodIt}) {
      if (it == params.end()) {
        return rxcpp::rxs::just(MakeErrorRedirect(redirectUri, ERROR_INVALID_REQUEST, "response_type, code_challenge, code_challenge_method required"));
      }
    }

    const std::string& responseType = (*responseTypeIt).value;
    codeChallenge = std::move((*codeChallengeIt).value);
    const std::string& codeChallengeMethod = (*codeChallengeMethodIt).value;

    if(responseType != RESPONSE_TYPE_CODE) {
      return rxcpp::rxs::just(MakeErrorRedirect(redirectUri, ERROR_UNSUPPORTED_RESPONSE_TYPE, "Only response type 'code' is supported."));
    }

    if(codeChallengeMethod != "S256") {
      return rxcpp::rxs::just(MakeErrorRedirect(redirectUri, ERROR_INVALID_REQUEST, "Only code challenge type 'S256' is supported"));
    }

    if (auto longLivedValidityIt = params.find("long_lived_validity");
        longLivedValidityIt != params.end()) {
      longLivedValidityStr = (*longLivedValidityIt).value;
    }
  }
  catch (std::exception& e) {
    PEP_LOG(LogTag, Severity::Error) << "Unexpected error: " << e.what();
    return rxcpp::rxs::just(MakeErrorRedirect(redirectUri, ERROR_SERVER_ERROR, SERVER_ERROR_DESCRIPTION));
  }

  return authserverBackend_->findUserGroupsAndStorePrimaryIdIfMissing(primaryUid, alternativeUids)
      .map([redirectUri, redirectUriString, request, clientId, humanReadableUid, codeChallenge, longLivedValidityStr, self=shared_from_this()](std::optional<std::vector<UserGroup>> groups) {
    if(!groups){
      return MakeErrorRedirect(redirectUri, ERROR_ACCESS_DENIED, "Unknown user");
    }
    if(groups->empty()) {
      return MakeErrorRedirect(redirectUri, ERROR_ACCESS_DENIED, "The user is not in any user groups");
    }

    UserGroup group = groups->front();
    if(groups->size() > 1) {
      auto formData = request.getBodyAsFormData();
      auto groupQuery = formData.find("user_group");
      if(groupQuery != formData.end()) {
        const auto& selectedGroup = groupQuery->second;
        auto foundGroup = std::ranges::find_if(*groups, [&selectedGroup](const UserGroup& group){ return group.name == selectedGroup; });
        if(foundGroup == groups->end()) {
          PEP_LOG(LogTag, Severity::Warning) << "Trying to login with group '" << selectedGroup << "', but user is not a member of that group.";
          return MakeErrorRedirect(redirectUri, ERROR_ACCESS_DENIED, "User is not a member of selected group");
        }
        group = *foundGroup;
      }
      else {
        std::ostringstream body;
        body << BEGIN_GROUP_SELECTION_TEMPLATE;
        std::set<std::string> sortedGroups;
        std::ranges::transform(*groups, std::inserter(sortedGroups, sortedGroups.begin()), [](const auto& g) {return g.name;});
        for(auto& g : sortedGroups) {
          body << "<option>" << g << "</option>";
        }
        body << END_GROUP_SELECTION_TEMPLATE;
        return HTTPResponse("200 OK", std::move(body).str());
      }
    }

    std::optional<std::chrono::seconds> validityDuration;
    if (longLivedValidityStr) {
      auto maxValidity = group.maxAuthValidity;
      if(!maxValidity) {
        return MakeErrorRedirect(redirectUri, ERROR_ACCESS_DENIED, "User is not allowed to request long-lived tokens");
      }
      if(boost::iequals(*longLivedValidityStr, "max")) {
        validityDuration = maxValidity;
      }
      else {
        validityDuration = std::chrono::seconds(boost::lexical_cast<int64_t>(*longLivedValidityStr));
        if(*validityDuration > *maxValidity) {
          return MakeErrorRedirect(redirectUri, ERROR_ACCESS_DENIED, "User is not allowed to request long-lived tokens for the requested duration");
        }
      }
    }

    std::string code = EncodeBase64Url(RandomString(32));
    self->addActiveGrant(code, Grant(clientId, humanReadableUid,  std::move(group), redirectUriString, codeChallenge, validityDuration));
    url returnUri = redirectUri;
    returnUri.params().set("code", code);

    return HTTPResponse("302 Found", "", {{"Location", std::string(returnUri.buffer())}});
  }).on_error_resume_next([redirectUri](std::exception_ptr ep) {
    try {
      std::rethrow_exception(ep);
    } catch (const Error& e) {
      return rxcpp::rxs::just(MakeErrorRedirect(redirectUri, ERROR_SERVER_ERROR, e.what()));
    } catch(const std::exception& e) {
      PEP_LOG(LogTag, Severity::Error) << "Unexpected error: " << e.what();
      return rxcpp::rxs::just(MakeErrorRedirect(redirectUri, ERROR_SERVER_ERROR, SERVER_ERROR_DESCRIPTION));
    }
  });
}

HTTPResponse OAuthProvider::handleTokenRequest(HTTPRequest request, std::string remoteIp) {
  try {
    auto formData = request.getBodyAsFormData();
    for(auto& p : {"client_id", "redirect_uri", "grant_type", "code", "code_verifier"}) {
      if(formData.find(p) == formData.end()) {
        std::ostringstream oss;
        oss << p << " required";
        return MakeErrorJsonHttpResponse(ERROR_INVALID_REQUEST, std::move(oss).str());
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
    if(grant->codeChallenge != EncodeBase64Url(Sha256().digest(codeVerifier))) {
      return MakeErrorJsonHttpResponse(ERROR_INVALID_GRANT, "Code challenge failed");
    }
    if(grant->clientId != clientId) {
      return MakeErrorJsonHttpResponse(ERROR_INVALID_REQUEST, "client_id does not match the known client_id for this code");
    }
    if(grant->redirectUri != redirectUri) {
      return MakeErrorJsonHttpResponse(ERROR_INVALID_REQUEST, "redirect_uri does not match the known redirect_uri for this code");
    }

    OAuthToken token = authserverBackend_->getToken(grant->humanReadableId, grant->usergroup, grant->validity);

    boost::property_tree::ptree responseData;
    responseData.add("access_token", token.getSerializedForm());
    responseData.add("token_type", "bearer");
    responseData.add("expires_in", std::chrono::seconds{1min}.count());
    std::ostringstream responseStream;
    boost::property_tree::write_json(responseStream, responseData);

    HTTPMessage::HeaderMap responseHeaders{
      // It should be fine to put "*" here, because a website can only obtain a valid code for this endpoint by
      // performing the whole authentication procedure, in which case they should receive a token.
      {"Access-Control-Allow-Origin", "*"},
    };
    return MakeHttpResponse("200 OK", std::move(responseStream).str(), "application/json", std::move(responseHeaders));
  }
  catch (const Error& err) {
    return MakeErrorJsonHttpResponse(ERROR_SERVER_ERROR, err.what());
  }
  catch(...) {
    return MakeErrorJsonHttpResponse(ERROR_SERVER_ERROR, SERVER_ERROR_DESCRIPTION);
  }
}

HTTPResponse OAuthProvider::handleCodeRequest(HTTPRequest request, std::string remoteIp) {
  std::string status="200 OK";
  std::optional<std::string> error;
  std::string result;
  const auto& uri = request.uri();
  const auto& params = uri.params();
  if (auto exception = OAuthError::TryRead(uri); exception.has_value()) {
    error = exception->what();
  }
  else if (auto codeIt = params.find("code"); codeIt != params.end()) {
    const std::string& code = (*codeIt).value;
    auto grantIterator = activeGrants_.find(code);
    if(grantIterator == activeGrants_.end()) {
      status = "404 Not Found";
      error = "Unknown code";
    }
    else {
      TemplateEnvironment::Data data {
        {"code", grantIterator->first},
        {"validity", chrono::ToString(activeGrantExpiration_)}
      };

      result = templates_.renderTemplate("authserver/code.html.j2", data);
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

    result = templates_.renderTemplate("common/page.html.j2", data);
  }
  return MakeHttpResponse(status, result, "text/html");
}

const std::vector<url>& OAuthProvider::getRegisteredRedirectURIs(const std::string& clientId) {
  //We currently only support one client_id. There are no plans to change this, so no need to make this more complicated for now.
  if (clientId == PepClientId) {
    return allowedRedirectUris_;
  }
  return Default<std::vector<url>>;
}

}
