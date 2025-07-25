#include <pep/castor/CastorClient.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Log.hpp>
#include <pep/castor/Study.hpp>
#include <pep/castor/Ptree.hpp>

#include <pep/crypto/Timestamp.hpp>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <rxcpp/operators/rx-concat_map.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-filter.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-take.hpp>
#include <rxcpp/operators/rx-on_error_resume_next.hpp>

#include <cmath>
#include <sstream>
#include <string>

static const std::string LOG_TAG ("CastorClient");
namespace pep {
namespace castor {

namespace {

const std::string CASTOR_429_RESPONSE_MESSAGE_HEADER = "Too many requests, retry after: ";

}

const int AuthenticationStatus::EXPIRY_MARGIN_SECONDS = 30;

bool AuthenticationStatus::authenticated() const {
  if (state != AUTHENTICATED) {
    return false;
  }
  assert(expires.has_value());
  return time(nullptr) < *expires - EXPIRY_MARGIN_SECONDS;
}

void CastorClient::Connection::reauthenticate() {
  LOG(LOG_TAG, info) << "Reauthenticating to Castor";
  auto clientID = getClient()->mClientId, clientSecret = getClient()->mClientSecret;
  if(clientID == "" || clientSecret == "")
    throw std::runtime_error("Must authenticate first to CastorClient.");
  authenticationSubject.get_subscriber().on_next(AuthenticationStatus(AUTHENTICATING));
  std::shared_ptr<HTTPRequest> request = makePost("/oauth/token", "grant_type=client_credentials&client_id=" + clientID + "&client_secret=" + clientSecret, false);
  request->setHeader("Content-Type", "application/x-www-form-urlencoded");
  sendPreAuthorizedRequest(request)
    .map([](HTTPResponse response){
      if(response.getStatusCode() != 200) {
        throw CastorException::FromErrorResponse(response, "in CastorClient::Connection::reauthenticate");
      }
      boost::property_tree::ptree responseJson;
      ReadJsonIntoPtree(responseJson, response.getBody());
      return AuthenticationStatus(responseJson.get<std::string>("access_token"), responseJson.get<int>("expires_in"));
    })
    .on_error_resume_next([](std::exception_ptr ep){
      LOG(LOG_TAG, error) << "Failed authenticating to Castor: " << rxcpp::rxu::what(ep);
      return rxcpp::observable<>::just(AuthenticationStatus(ep));
    })
    .subscribe([self = SharedFrom(*this)](AuthenticationStatus status){
      self->authenticationSubject.get_subscriber().on_next(status);
    });
}

rxcpp::observable<HTTPResponse> CastorClient::Connection::sendPreAuthorizedRequest(std::shared_ptr<HTTPRequest> request) {
  request->setHeader("Accept", "application/json");
  if(request->getMethod() == HTTPRequest::GET) {
    request->uri().params().set("page_size", std::to_string(pageSize));
  }
  return HTTPSClient::Connection::sendRequest(request);
}

rxcpp::observable<HTTPResponse> CastorClient::Connection::sendRequest(std::shared_ptr<HTTPRequest> request) {
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

rxcpp::observable<JsonPtr> CastorClient::Connection::handleCastorResponse(std::shared_ptr<HTTPRequest> request, const HTTPResponse& response) {
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
        self->sendCastorRequest(self->makeGet(self->pathFromUrl(*next)))
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
    auto when = Timestamp::FromXmlDateTime(xml).toTime_t();
    auto now = std::time(nullptr);
    auto diff = std::difftime(when, now); // in seconds

    // An observable that'll emit a FakeVoid when we can retry the request
    rxcpp::observable<FakeVoid> wait;
    if (diff < 0) { // No need to wait: e.g. processing or transmission took a while, or we've been sitting on a breakpoint
      wait = rxcpp::observable<>::just(FakeVoid());
    }
    else {
      // Just to be sure: wait 1 second longer than calculated, since message says to retry _after_ the specified time
      LOG(LOG_TAG, info) << "Castor requests throttled until " << xml
        << ": waiting for " << static_cast<uintmax_t>(diff) << " + 1 seconds";
      ++diff;

      /* Convert our number of seconds(represented as a double) to an std::chrono::duration<>.
       * Since we don't specify the 2nd template parameter, it defaults to std::ratio<1>,
       * which means "seconds". See https://en.cppreference.com/w/cpp/chrono/duration
       */
      std::chrono::duration<double> seconds(diff);

      // Convert our duration<> to the steady_clock::duration that RX's Timer operator wants
      assert(!std::isnan(seconds.count()));
      assert(!std::isinf(seconds.count()));
      // TODO: prevent undefined behavior when our double is "too large to be representable by the target's integer type": see https://en.cppreference.com/w/cpp/chrono/duration/duration_cast
      // TODO: prevent arithmetic overflow converting our number of seconds to steady_clock's tick period (e.g. nanoseconds)
      auto duration = std::chrono::duration_cast<std::chrono::steady_clock::duration>(seconds);
      wait = rxcpp::observable<>::timer(duration)
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
    std::string info = "in CastorClient::Connection::sendCastorRequest.";
    auto expires = this->authenticationSubject.get_value().expires;
    if (expires.has_value()) {
      info += " OAuth2 expires at: " + boost::posix_time::to_simple_string(boost::posix_time::from_time_t(*expires));
    }
    info += "\nRequest:\n" + request->toString();
    throw CastorException::FromErrorResponse(response, info);
  }
  }
}

rxcpp::observable<JsonPtr> CastorClient::Connection::sendCastorRequest(std::shared_ptr<HTTPRequest> request) {
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
