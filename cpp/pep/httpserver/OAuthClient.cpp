#include <pep/httpserver/OAuthClient.hpp>
#include <pep/utils/Random.hpp>
#include <pep/utils/Base64.hpp>
#include <pep/utils/Win32Api.hpp>
#include <pep/networking/HTTPSClient.hpp>

#include <cassert>
#include <optional>
#include <sstream>

#include <rxcpp/operators/rx-map.hpp>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#ifndef _WIN32
#include <boost/process/system.hpp>
#include <boost/process/search_path.hpp>
#endif
#include <pep/async/CreateObservable.hpp>
#include <pep/utils/Hasher.hpp>
#include <pep/async/OnAsio.hpp>
#include <pep/utils/Sha.hpp>

#include <pep/utils/UriEncode.hpp>

namespace {
  const std::string LOG_TAG = "OAuthClient";

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

  std::string GetStatusHtml(const std::optional<std::string> failure = std::nullopt) {
    std::string color = "006097";
    std::string negation;
    std::string feedback = "Please close your browser";
    if (failure.has_value()) {
      color = "FF0000";
      negation = "<em>not</em>";
      feedback = *failure;
    }
    return
      "<div style=\"font: 20px Roboto,arial,sans-serif; text-align: center; background-color: #" + color + "; color: white; display: table; position: absolute; height: 100%; width: 100%;\">"
      "<div style=\"display: table-cell; vertical-align: middle;\">"
      "<div style=\"margin-left: auto; margin-right: auto; width: auto;\">"
      "<img src='https://pep.cs.ru.nl/img/PEPwit.png' alt='PEP logo' style='margin: 30px;'><br>You have " + negation + " been logged in. " + feedback + "."
      "</div>"
      "</div>"
      "</div>";
  }

  void OpenBrowser(const pep::HTTPRequest::Uri& url) {
    if(url.isRelative())
      throw std::runtime_error("Can not open relative URLs");
    bool success = false;
    try {
#if defined(_WIN32)
      // The alternative for "open" and "xdg-open" on windows is "start". However that is a CMD-builtin, and not an executable we can call using boost::process
      // Running a command in CMD potentially exposes us to security issues, so instead we use our own win32api
      pep::win32api::StartProcess(url.toString());
      success = true;
#elif defined(__linux__)
      std::string command = "xdg-open";
#elif defined(__APPLE__) && defined(__MACH__)
      std::string command = "open";
#endif
#if defined(__linux__) || ( defined(__APPLE__) && defined(__MACH__) )
      std::filesystem::path cmdPath = boost::process::search_path(command);
      if(!cmdPath.empty()) {
        int exitCode = boost::process::system(cmdPath, url.toString());
        if(exitCode != 0) {
          LOG(LOG_TAG, pep::warning) << "Failed to open browser. '" << command << "' returned exit code: " << exitCode;
        }
        else {
          success = true;
        }
      }
#endif
    }
    catch(std::exception& e) {
      LOG(LOG_TAG, pep::warning) << "Failed to open browser: " << e.what();
    }

    if(!success) {
      std::cout << "Could not open a browser. Please open " << url.toString() << " in your browser.";
    }
  }
}

namespace pep {
OAuthClient::OAuthError::OAuthError(std::string error, std::string description)
  : mError(error), mDescription(description), mWhat((description + " (" + error + ")")) {}

const char* OAuthClient::OAuthError::what() const noexcept {
  return mWhat.c_str();
}

OAuthClient::OAuthClient(Parameters parameters)
  : mIoService(parameters.ioService), mCodeVerifier(GenerateCodeVerifier().c_str()), mLimitedEnvironment(parameters.limitedEnvironment), mLongLived(parameters.longLived), mValidityDuration(parameters.validityDuration) {
  mRequestUrl = parameters.config.get<std::string>("RequestURL");
  mTokenUrl = parameters.config.get<std::string>("TokenURL");
  mCaCertFilepath = parameters.config.get<std::optional<std::filesystem::path>>("CaCertFilePath");
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
}

rxcpp::observable<std::string> OAuthClient::run() {
  return CreateObservable<std::string>([self=shared_from_this()](rxcpp::subscriber<std::string> subscriber) {
    HTTPRequest::Uri uri(self->mRequestUrl);
    uri.setQuery("client_id", "123");
    uri.setQuery("response_type", "code");
    uri.setQuery("code_challenge", encodeBase64URL(Sha256().digest(self->mCodeVerifier)));
    uri.setQuery("code_challenge_method", "S256");
    if(self->mLongLived) {
      if(self->mValidityDuration) {
        uri.setQuery("long_lived_validity", std::to_string(self->mValidityDuration->count()));
      }
      else {
        uri.setQuery("long_lived_validity", "max");
      }
    }
    if(self->mLimitedEnvironment) {
      uri.setQuery("redirect_uri", "/code");
      std::cerr << "Please open " << uri.toString() << " in your browser." << std::endl;
      std::cerr << "Paste your code here: ";
      std::string code;
      std::getline(std::cin, code);
      self->doTokenRequest(code, subscriber);
    }
    else {
      uri.setQuery("redirect_uri", "http://127.0.0.1:16515/");
      self->mHttpServer = std::make_shared<HTTPServer>(16515, self->mIoService);
      self->mHttpServer->registerHandler("/", true, [subscriber, self](HTTPRequest localhostRequest) {
        boost::asio::post(*self->mIoService, [self](){
          self->mHttpServer = nullptr;
        });
        std::optional<std::string> error;
        if (localhostRequest.hasQuery("error")) {
          if(localhostRequest.hasQuery("error_description")) {
            error = localhostRequest.query("error_description") + " (";
          }
          error = error.value_or("") + localhostRequest.query("error");
          if(localhostRequest.hasQuery("error_description")) {
            error = *error + ")";
          }
        }
        else if(localhostRequest.hasQuery("code")) {
          self->mIoService->post(std::bind(&OAuthClient::doTokenRequest, self.get(),
            localhostRequest.query("code"),
            subscriber));
        }
        else {
          error = "An unexpected error occured";
        }

        return HTTPResponse("200 OK", GetStatusHtml(error));
      }, "GET");

      OpenBrowser(uri.toString());
    }
  }).subscribe_on(observe_on_asio(*mIoService));
}

void OAuthClient::doTokenRequest(const std::string& code, const rxcpp::subscriber<std::string>& subscriber) {
  std::ostringstream ossBody;
  ossBody << "client_id=123";
  if(mLimitedEnvironment) {
    ossBody << "&redirect_uri=" << UriEncode("/code", true);
  }
  else {
    ossBody << "&redirect_uri=" << UriEncode("http://127.0.0.1:16515/", true);
  }
  ossBody << "&grant_type=authorization_code";
  ossBody << "&code=" << UriEncode(code, true);
  ossBody << "&code_verifier=" << UriEncode(mCodeVerifier, true);
  auto request = std::make_shared<HTTPRequest>(HTTPRequest::POST, HTTPRequest::Uri(mTokenUrl), ossBody.str());

  HTTPSClient::SendRequest(request, mIoService, mCaCertFilepath).map([](HTTPResponse response) {
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
        oss << "An unexpected error occured while requesting a token." << std::endl;
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
  }).subscribe(subscriber);
}
}
