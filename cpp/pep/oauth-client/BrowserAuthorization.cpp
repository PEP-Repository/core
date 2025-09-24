#include <pep/oauth-client/BrowserAuthorization.hpp>

#include <pep/auth/OAuthError.hpp>
#include <pep/async/CreateObservable.hpp>
#include <pep/httpserver/HTTPServer.hpp>
#include <pep/utils/Log.hpp>

#include <filesystem>
#include <format>

#include <boost/asio/io_context.hpp>
#include <boost/url/url_view.hpp>

#include <pep/utils/Win32Api.hpp>
#if defined(__linux__) || ( defined(__APPLE__) && defined(__MACH__) )
// TODO: switch to boost.process v2, since v1 is deprecated: see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2646
# include <boost/process/v1/system.hpp>
# include <boost/process/v1/search_path.hpp>
#endif

using namespace pep;

namespace {
const std::string LOG_TAG = "BrowserAuthorization";

// See OAuthProvider::getRegisteredRedirectURIs
constexpr std::uint16_t ListenPort{16515};

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
      "<div style=\"font: 20px Roboto,arial,sans-serif; text-align: center; background-color: #" + color +
      "; color: white; display: table; position: absolute; height: 100%; width: 100%;\">"
      "<div style=\"display: table-cell; vertical-align: middle;\">"
      "<div style=\"margin-left: auto; margin-right: auto; width: auto;\">"
      "<img src='https://pep.cs.ru.nl/img/PEPwit.png' alt='PEP logo' style='margin: 30px;'><br>You have " + negation +
      " been logged in. " + feedback + "."
      "</div>"
      "</div>"
      "</div>";
}

void OpenBrowser(boost::urls::url_view url) {
  LOG(LOG_TAG, pep::debug) << "Opening in browser: " << url;
  if (!url.is_path_absolute())
    throw std::runtime_error("Can not open relative URLs");
  bool success = false;
  try {
#if defined(_WIN32)
    // The alternative for "open" and "xdg-open" on windows is "start". However that is a CMD-builtin, and not an executable we can call using boost::process
    // Running a command in CMD potentially exposes us to security issues, so instead we use our own win32api
    pep::win32api::StartProcess(std::string(url.buffer()));
    success = true;
#elif defined(__linux__)
    std::string command = "xdg-open";
#elif defined(__APPLE__) && defined(__MACH__)
    std::string command = "open";
#endif
#if defined(__linux__) || ( defined(__APPLE__) && defined(__MACH__) )
    std::filesystem::path cmdPath = boost::process::v1::search_path(command);
    if (!cmdPath.empty()) {
      int exitCode = boost::process::v1::system(cmdPath, std::string(url.buffer()));
      if (exitCode != 0) {
        LOG(LOG_TAG, pep::warning) << "Failed to open browser. '" << command << "' returned exit code: " << exitCode;
      } else {
        success = true;
      }
    }
#endif
  } catch (std::exception &e) {
    LOG(LOG_TAG, pep::warning) << "Failed to open browser: " << e.what();
  }

  if (!success) {
    std::cout << "Could not open a browser. Please open " << url << " in your browser." << std::endl;
  }
}

}

rxcpp::observable<AuthorizationResult> pep::BrowserAuthorization(
  std::shared_ptr<boost::asio::io_context> io_context,
  std::function<std::string (std::string redirectUri)> getAuthorizeUri
) {
  auto authorizeUri = getAuthorizeUri(std::format("http://localhost:{}/", ListenPort));
  return CreateObservable<AuthorizationResult>([io_context, authorizeUri](rxcpp::subscriber<AuthorizationResult> subscriber) {
    std::shared_ptr<HTTPServer> httpServer = std::make_shared<HTTPServer>(ListenPort, io_context);
    httpServer->registerHandler("/", true, [io_context, subscriber, httpServer](HTTPRequest localhostRequest, std::string remoteIp) {
      std::optional<AuthorizationResult> result;
      std::optional<std::string> failure;

      const auto& uri = localhostRequest.uri();
      const auto& params = uri.params();
      if (auto error = OAuthError::TryRead(uri); error.has_value()) {
        result = AuthorizationResult::Failure(std::make_exception_ptr(*error));
        failure = error->what();
      } else if (auto codeIt = params.find("code"); codeIt != params.end()) {
        result = AuthorizationResult::Success((*codeIt).value);
      } else {
        auto error = OAuthError("Authorization failed", "An unexpected error occurred");
        result = AuthorizationResult::Failure(std::make_exception_ptr(error));
        failure = error.what();
      }

      assert(result.has_value());
      assert(result->successful() != failure.has_value());

      (void)rxcpp::observable<>::just(*result).subscribe(subscriber);

      // Only stop after we returned an HTTP response
      post(*io_context, [httpServer] {
        httpServer->asyncStop();
        });

      return HTTPResponse("200 OK", GetStatusHtml(failure));
    }, "GET");

    OpenBrowser(boost::urls::url(authorizeUri));
  });
}
