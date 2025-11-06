#include <pep/castor/CastorClient.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/async/FakeVoid.hpp>
#include <pep/async/RxRequireCount.hpp>
#include <pep/async/RxInstead.hpp>
#include <pep/utils/Log.hpp>
#include <pep/castor/Study.hpp>
#include <pep/castor/Ptree.hpp>
#include <pep/crypto/Timestamp.hpp>

#include <rxcpp/operators/rx-concat_map.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-filter.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-reduce.hpp>
#include <rxcpp/operators/rx-take.hpp>
#include <rxcpp/operators/rx-on_error_resume_next.hpp>

#include <cmath>
#include <sstream>
#include <string>

using namespace std::literals;

static const std::string LOG_TAG ("CastorClient");
namespace pep {
namespace castor {

namespace {

const std::string CASTOR_429_RESPONSE_MESSAGE_HEADER = "Too many requests, retry after: ";

std::shared_ptr<networking::HttpClient> CreateHttpClient(boost::asio::io_context& ioContext, const EndPoint& endPoint, std::optional<std::filesystem::path> caCertFilepath) {
  networking::HttpClient::Parameters parameters(ioContext, true, endPoint);
  parameters.caCertFilepath(std::move(caCertFilepath));
  return networking::HttpClient::Create(std::move(parameters));
}

}

const std::chrono::seconds AuthenticationStatus::EXPIRY_MARGIN{30};
const std::string CastorClient::BASE_PATH = "/api/";

bool AuthenticationStatus::authenticated() const {
  if (state != AUTHENTICATED) {
    return false;
  }
  assert(expires.has_value());
  return TimeNow() < *expires - EXPIRY_MARGIN;
}

CastorClient::CastorClient(boost::asio::io_context& ioContext, const EndPoint& endPoint, std::string clientId, std::string clientSecret, std::optional<std::filesystem::path> caCertFilepath)
  : mHttp(CreateHttpClient(ioContext, endPoint, std::move(caCertFilepath))), mClientId(std::move(clientId)), mClientSecret(std::move(clientSecret)) {
  if (mClientId.empty()) {
    throw std::runtime_error("clientID must be set");
  }
  if (mClientSecret.empty()) {
    throw std::runtime_error("clientSecret must be set");
  }
  mOnRequestForwarding = mHttp->onRequest.subscribe([this](std::shared_ptr<const HTTPRequest> request) {onRequest.notify(request); });
}

void CastorClient::shutdown() {
  mOnRequestForwarding.cancel();
  if (mHttp != nullptr) {
    mHttp->shutdown();
    mHttp = nullptr;
  }
}

void CastorClient::start() {
  if (this->status() > Status::initialized || mHttp == nullptr) {
    throw std::runtime_error("Can't (re)start a finalized Castor client");
  }
  mHttp->start();
}

void CastorClient::reauthenticate() {
  LOG(LOG_TAG, info) << "Reauthenticating to Castor";
  authenticationSubject.get_subscriber().on_next(AuthenticationStatus(AUTHENTICATING));
  std::shared_ptr<HTTPRequest> request = makePost("/oauth/token", "grant_type=client_credentials&client_id=" + mClientId + "&client_secret=" + mClientSecret, false);
  request->setHeader("Content-Type", "application/x-www-form-urlencoded");
  sendPreAuthorizedRequest(request)
    .map([](HTTPResponse response){
      if(response.getStatusCode() != 200) {
        throw CastorException::FromErrorResponse(response, "in CastorClient::reauthenticate");
      }
      boost::property_tree::ptree responseJson;
      ReadJsonIntoPtree(responseJson, response.getBody());
      using namespace std::chrono;
      return AuthenticationStatus(responseJson.get<std::string>("access_token"),
          seconds{responseJson.get<seconds::rep>("expires_in")});
    })
    .on_error_resume_next([](std::exception_ptr ep){
      LOG(LOG_TAG, error) << "Failed authenticating to Castor: " << rxcpp::rxu::what(ep);
      return rxcpp::observable<>::just(AuthenticationStatus(ep));
    })
    .subscribe([self = SharedFrom(*this)](AuthenticationStatus status){
      self->authenticationSubject.get_subscriber().on_next(status);
    });
}

rxcpp::observable<HTTPResponse> CastorClient::sendPreAuthorizedRequest(std::shared_ptr<HTTPRequest> request) {
  request->setHeader("Accept", "application/json");
  if(request->getMethod() == networking::HttpMethod::GET) {
    request->uri().params().set("page_size", std::to_string(PAGE_SIZE));
  }
  return mHttp->sendRequest(*request);
}

/*!
 * \brief Make a GET Request
 *
 * \param path Path to the resource to get
 * \param useBasePath Whether \p path should be relative to the \ref setBasePath "base path" or not
 * \return The created Request
 */
std::shared_ptr<HTTPRequest> CastorClient::makeGet(const std::string& path, const bool& useBasePath) {
  return MakeSharedCopy(mHttp->makeRequest(networking::HttpMethod::GET, (useBasePath ? BASE_PATH : "") + path));
};

/*!
 * \brief Make a POST Request
 *
 * \param path Path to the resource to post
 * \param body Body of the Request
 * \param useBasePath Whether \p path should be relative to the \ref setBasePath "base path" or not
 * \return The created Request
 */
std::shared_ptr<HTTPRequest> CastorClient::makePost(const std::string& path,
  const std::string& body,
  const bool& useBasePath) {
  auto result = MakeSharedCopy(mHttp->makeRequest(networking::HttpMethod::POST, (useBasePath ? BASE_PATH : "") + path));
  assert(result->getBodyparts().empty());
  result->getBodyparts().emplace_back(MakeSharedCopy(body));
  return result;
}

rxcpp::observable<HTTPResponse> CastorClient::sendRequest(std::shared_ptr<HTTPRequest> request) {
  if (!authenticationSubject.get_value().authenticated()
    && authenticationSubject.get_value().state != AUTHENTICATING) {
    reauthenticate();
  }

  return authenticationStatus()
    .filter([](AuthenticationStatus status) { return status.state == AUTHENTICATION_ERROR || status.authenticated(); })
    .first()
    .flat_map([this, request](AuthenticationStatus status) {
      if (status.state == AUTHENTICATION_ERROR) {
        std::rethrow_exception(status.exceptionPtr);
      }

      request->setHeader("Authorization", "Bearer " + status.token);
      return sendPreAuthorizedRequest(request);
    });
}

rxcpp::observable<JsonPtr> CastorClient::handleCastorResponse(std::shared_ptr<HTTPRequest> request, const HTTPResponse& response) {
  switch (response.getStatusCode()) {
  case 200: // OK
  case 201: // Created
  {
    std::shared_ptr<boost::property_tree::ptree> responseJson = std::make_shared<boost::property_tree::ptree>();
    ReadJsonIntoPtree(*responseJson, response.getBody());

    /* Followup page retrieval (below) doesn't use RX's "merge" or "start_with" operator to combine this response with
     * followup page responses because the resulting observable can "consist of" so many ptrees that it takes up (too)
     * much memory. When e.g. the "concat_map" operator is used on the observable that we return, all those ptrees are
     * kept in memory until _after_ all of them have been processed. This is caused by the "concat_map" operator not
     * discarding its (nested) subscriptions until after the outer subscription is discarded (i.e. when the observable
     * has been exhausted).
     * So instead of letting a single observable manage its own subscriptions, we use manual "daisy chaining" to ensure
     * that only a single subscription is active at any time, causing ptrees to be discarded as soon as they have been
     * processed.
     */
    return CreateObservable<JsonPtr>([self = SharedFrom(*this), current = std::static_pointer_cast<const boost::property_tree::ptree>(responseJson)](rxcpp::subscriber<JsonPtr> subscriber) {
      subscriber.on_next(current);
      auto next = current->get_optional<std::string>("_links.next.href");
      if (!next) {
        subscriber.on_completed();
      }
      else {
        self->sendCastorRequest(self->makeGet(self->mHttp->pathFromUrl(boost::urls::url(*next)), false))
          .subscribe(
            [subscriber](JsonPtr followup) {subscriber.on_next(followup); },
            [subscriber](std::exception_ptr ep) {subscriber.on_error(ep); },
            [subscriber]() {subscriber.on_completed(); });
      }
    });
  }

  case 429: // Too Many Requests
  {
    std::shared_ptr<boost::property_tree::ptree> responseJson = std::make_shared<boost::property_tree::ptree>();
    ReadJsonIntoPtree(*responseJson, response.getBody()); // E.g. {"success":false,"errors":[{"id":"fa420c23","code":"CODE_QUOTA_EXCEEDED","message":"Too many requests, retry after: 2023-01-31T00:32:32+00:00","data":[]}]}

    // Determine when we're allowed to retry by parsing the (one and only) error in the response JSON
    const auto& errors = responseJson->get_child("errors");
    if (errors.size() != 1U) {
      throw CastorException::FromErrorResponse(response, "Expected exactly one error in Castor 429 response; got " + std::to_string(errors.size()));
    }
    // TODO: don't parse human-readable "message" to extract timestamp
    auto message = errors.front().second.get<std::string>("message"); // E.g. "Too many requests, retry after: 2023-01-31T00:32:32+00:00"
    if (!message.starts_with(CASTOR_429_RESPONSE_MESSAGE_HEADER)) {
      throw CastorException::FromErrorResponse(response, "Castor 429 response contains unparseable retry time message");
    }
    auto xml = message.substr(CASTOR_429_RESPONSE_MESSAGE_HEADER.size());

    // Calculate the time to wait before retrying
    auto retryWhen = TimestampFromXmlDataTime(xml);

    // An observable that'll emit a FakeVoid when we can retry the request
    rxcpp::observable<FakeVoid> wait;
    if (TimeNow() > retryWhen) { // No need to wait: e.g. processing or transmission took a while, or we've been sitting on a breakpoint
      wait = rxcpp::observable<>::just(FakeVoid());
    }
    else {
      // Just to be sure: wait 1 second longer than calculated, since message says to retry _after_ the specified time
      retryWhen += 1s;
      LOG(LOG_TAG, info) << "Castor requests throttled until " << xml;

      // We need to use a duration instead of a time_point as Rx wants a steady_clock time
      wait = rxcpp::observable<>::timer(retryWhen - TimeNow())
        .op(RxGetOne("emissions from RX timer"))
        .op(RxInstead(FakeVoid()));
    }

    // Re-send the request when the wait is over
    return wait.concat_map([self = SharedFrom(*this), request](const FakeVoid&) {
      return self->sendCastorRequest(request);
    });
  }

  default: // Not an HTTP status code that we can deal with
  {
    std::string info = "in CastorClient::sendCastorRequest.";
    auto expires = this->authenticationSubject.get_value().expires;
    if (expires.has_value()) {
      info += " OAuth2 expires at: " + TimestampToXmlDateTime(*expires);
    }
    info += "\nRequest:\n" + request->toString();
    throw CastorException::FromErrorResponse(response, info);
  }
  }
}

rxcpp::observable<JsonPtr> CastorClient::sendCastorRequest(std::shared_ptr<HTTPRequest> request) {
  if(!request->hasHeader("Content-Type")) {
    request->setHeader("Content-Type", "application/json");
  }
  auto parseResponseLambda = [self = SharedFrom(*this), request](const HTTPResponse& response){
    return self->handleCastorResponse(request, response);
  };
  return sendRequest(request).concat_map(parseResponseLambda).on_error_resume_next([self = SharedFrom(*this), request, parseResponseLambda](std::exception_ptr ep){
    try {
      std::rethrow_exception(ep);
    }
    catch(CastorException &ex) {
      LOG(LOG_TAG, debug) << "Castor Error. Retrying once. Error message: " << ex.what();
      return self->sendRequest(request).concat_map(parseResponseLambda);
    }
  });
}

}
}
