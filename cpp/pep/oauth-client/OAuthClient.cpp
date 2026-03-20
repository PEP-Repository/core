#include <pep/oauth-client/OAuthClient.hpp>

#include <pep/auth/OAuthError.hpp>
#include <pep/utils/Random.hpp>
#include <pep/utils/Base64.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/Win32Api.hpp>
#include <pep/networking/HttpClient.hpp>

#include <cassert>
#include <sstream>

#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <pep/async/CreateObservable.hpp>
#include <pep/async/OnAsio.hpp>
#include <pep/utils/OpenSSLHasher.hpp>

namespace {
  std::string GenerateCodeVerifier() {
    // While the code verifier can be any random 43-128 char alphanumeric (plus -/./_/~) string,
    // the recommended generation method is base64url-encoding 32 bytes.
    // See https://datatracker.ietf.org/doc/html/rfc7636#section-4.1.

    return pep::EncodeBase64Url(pep::RandomString(32));
  }

  const std::string ClientId = "123";
}

namespace pep {

OAuthClient::OAuthClient(Parameters parameters)
  : mIoContext(std::move(parameters.io_context)),
    mAuthorizationMethod(std::move(parameters.authorizationMethod)),
    mLongLived(parameters.longLived),
    mValidityDuration(parameters.validityDuration) {
  assert(mAuthorizationMethod && "No authorization method provided");
  mRequestUrl = parameters.config.get<std::string>("RequestURL");
  mTokenUrl = parameters.config.get<std::string>("TokenURL");
  mCaCertFilepath = parameters.config.get<std::optional<std::filesystem::path>>("CaCertFilePath");
}

boost::urls::url OAuthClient::getAuthorizationUri() const {
  boost::urls::url uri(mRequestUrl);
  uri.set_params({
    {"client_id", ClientId},
    {"response_type", "code"},
    {"code_challenge", EncodeBase64Url(Sha256().digest(mCodeVerifier))},
    {"code_challenge_method", "S256"},
    {"redirect_uri", mRedirectUrl},
  });
  if (mLongLived) {
    if (mValidityDuration) {
      uri.params().set("long_lived_validity", std::to_string(mValidityDuration->count()));
    } else {
      uri.params().set("long_lived_validity", "max");
    }
  }
  return uri;
}

rxcpp::observable<AuthorizationResult> OAuthClient::run() {
#ifdef WIN32
  /* Pass auth server's URL through the Windows API so that the root CA is added to the certificate store.
   * See https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2107#note_28826
   */
  if(!mCaCertFilepath.has_value()) {
    auto path = win32api::GetUniqueTemporaryPath();
    win32api::Download(mRequestUrl, path);
    std::filesystem::remove(path);
  }
#endif

  mCodeVerifier = GenerateCodeVerifier();
  return mAuthorizationMethod(mIoContext, [self = shared_from_this()](std::string redirectUri) -> std::string {
        self->mRedirectUrl = std::move(redirectUri);
        return std::string(self->getAuthorizationUri().buffer());
      })
      .subscribe_on(observe_on_asio(*mIoContext))
      .flat_map([self = shared_from_this()](const AuthorizationResult &result) -> rxcpp::observable<AuthorizationResult> {
        if (!result) {
          return rxcpp::observable<>::just(result);
        }
        return self->doTokenRequest(*result)
          .map([](std::string token) { return AuthorizationResult::Success(std::move(token)); });
      });
}

rxcpp::observable<std::string> OAuthClient::doTokenRequest(const std::string& code) {
  std::string body;
  {
    boost::urls::url form;
    form.set_params({
      {"client_id", ClientId},
      // Repeat same redirect_uri
      {"redirect_uri", mRedirectUrl},
      {"grant_type", "authorization_code"},
      {"code", code},
      {"code_verifier", mCodeVerifier},
    });
    body = std::string(form.encoded_query());
  }
  networking::HttpClient::Parameters parameters(*mIoContext, boost::urls::url(mTokenUrl));
  parameters.caCertFilepath(mCaCertFilepath);
  auto client = networking::HttpClient::Create(std::move(parameters));

  HTTPRequest request(networking::HttpMethod::POST, boost::urls::url(mTokenUrl), std::move(body),
    HTTPMessage::HeaderMap{{"Content-Type", "application/x-www-form-urlencoded"}});

  client->start();
  return client->sendRequest(request).map([client](HTTPResponse response) {
    client->shutdown();
    if(response.getStatusCode() != 200) {
      std::string error;
      std::string description;
      try {
        boost::property_tree::ptree errorJson;
        std::istringstream stream(response.getBody());
        boost::property_tree::read_json(stream, errorJson);
        error = errorJson.get<std::string>("error");
        description = errorJson.get<std::string>("error_description");
      }
      catch (...) {
        std::ostringstream oss;
        oss << "An unexpected error occurred while requesting a token." << std::endl;
        oss << "Status: " << response.getStatusCode() << " " << response.getStatusMessage();
        oss << "contents: " << response.getBody();
        throw std::runtime_error(std::move(oss).str());
      }
      throw OAuthError(error, description);
    }

    boost::property_tree::ptree bodyJson;
    std::istringstream stream(response.getBody());
    boost::property_tree::read_json(stream, bodyJson);
    std::string token = bodyJson.get<std::string>("access_token");

    return token;
  });
}
}
