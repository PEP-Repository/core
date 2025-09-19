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
#include <pep/utils/Hasher.hpp>
#include <pep/async/OnAsio.hpp>
#include <pep/utils/Sha.hpp>

namespace {
  class RandomByteProvider {
  public:
    virtual uint8_t get() = 0;
    virtual ~RandomByteProvider() = default;
  };

  class SimpleRandomByteProvider : public RandomByteProvider {
  private:
    size_t mBufferSize;
    std::vector<uint8_t> mBuffer;
    size_t mIndex;

  public:
    explicit SimpleRandomByteProvider(size_t bufferSize = 32U) : mBufferSize(bufferSize), mBuffer(bufferSize), mIndex(bufferSize) {}

    uint8_t get() override {
      // Reinitialize buffer if we're out of random values
      if (mIndex >= mBufferSize) {
        pep::RandomBytes(mBuffer.data(), mBufferSize);
        mIndex = 0U;
      }

      // Return next randomly produced value
      return mBuffer[mIndex++];
    }
  };

  class MaximizedRandomByteProvider : public RandomByteProvider {
  private:
    SimpleRandomByteProvider &mRawProvider;
    uint8_t mExcludedMaximum;
    uint8_t mRawMaximum;

  public:
    MaximizedRandomByteProvider(SimpleRandomByteProvider &rawProvider, uint8_t excludedMaximum) : mRawProvider(rawProvider), mExcludedMaximum(excludedMaximum) {
      if (mExcludedMaximum <= 1) {
        throw std::runtime_error("Randomization range must allow for multiple values");
      }

      // Prevent modulo bias (see https://stackoverflow.com/a/10984975)
      mRawMaximum = (std::numeric_limits<uint8_t>::max() / mExcludedMaximum) * mExcludedMaximum;
    }

    uint8_t get() override {
      uint8_t raw;
      do {
        raw = mRawProvider.get();
      } while (raw >= mRawMaximum);
      return raw % mExcludedMaximum;
    }
  };

  class RangedRandomByteProvider : public RandomByteProvider {
  private:
    uint8_t mMinimum;
    uint8_t mMaximum;
    MaximizedRandomByteProvider mImplementor;

  public:
    RangedRandomByteProvider(SimpleRandomByteProvider &rawProvider, uint8_t minimum, uint8_t maximum) : mMinimum(minimum), mMaximum(maximum), mImplementor(rawProvider, static_cast<uint8_t>(maximum - minimum + 1)) {
      if (mMaximum <= mMinimum) {
        throw std::runtime_error("Randomization range must allow for multiple values");
      }
    }

    uint8_t get() override {
      auto result = mImplementor.get() + mMinimum;
      assert(result >= mMinimum);
      assert(result <= mMaximum);
      return static_cast<uint8_t>(result);
    }
  };

  const size_t CODE_VERIFIER_MIN_CHARS = 43;
  const size_t CODE_VERIFIER_MAX_CHARS = 128;
  const std::string CODE_VERIFIER_ALLOWED_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-._~";
  const auto CODE_VERIFIER_ALLOWED_CHAR_COUNT = CODE_VERIFIER_ALLOWED_CHARS.length();

  std::string GenerateCodeVerifier() {
    SimpleRandomByteProvider rawProvider;
    size_t length{RangedRandomByteProvider(rawProvider, CODE_VERIFIER_MIN_CHARS, CODE_VERIFIER_MAX_CHARS).get()};
    auto result = std::string(length, '\0');

    auto provider = MaximizedRandomByteProvider(rawProvider, static_cast<uint8_t>(CODE_VERIFIER_ALLOWED_CHAR_COUNT));
    for (size_t i = 0; i < length; ++i) {
      auto index = provider.get();
      result[i] = CODE_VERIFIER_ALLOWED_CHARS[index];
    }
    return result;
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
    {"code_challenge", encodeBase64URL(Sha256().digest(mCodeVerifier))},
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
        throw std::runtime_error(oss.str());
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
