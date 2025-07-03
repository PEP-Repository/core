#include <pep/utils/Defer.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Log.hpp>
#include <pep/async/OnAsio.hpp>
#include <pep/networking/NetworkingSerializers.hpp>
#include <pep/networking/TLSMessageProtocol.hpp>
#include <pep/async/CreateObservable.hpp>
#include <pep/utils/MiscUtil.hpp>
#include <pep/async/RxEnsureProgress.hpp>
#include <pep/async/RxUtils.hpp>
#include <pep/utils/Shared.hpp>

#include <boost/asio/placeholders.hpp>
#include <boost/asio/read.hpp>
#include <boost/bind/bind.hpp>
#include <rxcpp/rx-lite.hpp>

using namespace std::chrono_literals;

namespace pep {

namespace {

const std::string LOG_TAG("MessageProtocol");

class ConnectionFailureException : public std::runtime_error {
private:
  boost::system::errc::errc_t mReason;

public:
  ConnectionFailureException(boost::system::errc::errc_t reason, const std::string& message) noexcept
    : std::runtime_error(message), mReason(reason) {
  }

  boost::system::errc::errc_t getReason() const noexcept { return mReason; }
};

class RequestRefusedException : public Error {
public:
  explicit inline RequestRefusedException(const std::string& reason) : Error(reason) {}
};

std::string GetIncompatibleVersionSummary(const std::optional<GitlabVersion>& version) {
  if (version == std::nullopt) {
    return "<unspecified>";
  }
  auto result = version->getSummary();
  if (result.empty()) {
    return "<empty>";
  }
  return result;
}

void LogIncompatibleVersionDetails(severity_level severity, const std::string& type, const std::optional<GitlabVersion>& remote, const std::optional<GitlabVersion>& local) {
  if (remote != std::nullopt || local != std::nullopt) {
    LOG(LOG_TAG, severity) << "- " << type << " versions"
      << ": remote = " << GetIncompatibleVersionSummary(remote)
      << "; local = " << GetIncompatibleVersionSummary(local);
  }
}

}

void TLSMessageProtocol::Connection::boostOnHeaderReceived(const boost::system::error_code& error, size_t bytes) {
  if (error) {
    LOG(LOG_TAG, severity_level::verbose)
      << " \\__ error! " << error << ", that is, " << error.message();
    return this->onConnectFailed(error);
  }

  try {
    assert(bytes == sizeof(mMessageInHeader));
    auto header = MessageHeader::Decode(mMessageInHeader);
    auto length = header.length();

    if (length > MAX_SIZE_OF_MESSAGE) {
      LOG(LOG_TAG, severity_level::error)
        << "TLSMessageProtocol::Connection::boostOnHeaderReceived: "
        << "refusing " << length << "-byte message from " << describe()
        << " because it's larger than the maximum of " << MAX_SIZE_OF_MESSAGE << " bytes";
      return this->onConnectFailed(boost::asio::error::message_size);
    }

    LOG(LOG_TAG, severity_level::verbose)
      << "TLSMessageProtocol::Connection::boostOnHeaderReceived: "
      << "receiving " << length << "-byte message from " << describe();

    boost::asio::async_read(*socket,
      boost::asio::buffer(mMessageInBody.data(), length),
      boost::bind(&TLSMessageProtocol::Connection::boostOnMessageReceived,
        SharedFrom(*this),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));

  } catch (...) {
    LOG(LOG_TAG, pep::error) << "Failed to process message header: " << GetExceptionMessage(std::current_exception());
    this->onConnectFailed(boost::system::errc::make_error_code(boost::system::errc::errc_t::bad_message));
  }
}

void TLSMessageProtocol::Connection::ensureSend() {
  LOG(LOG_TAG, severity_level::verbose) << "TLSMessageProtocol::Connection::ensureSend (mState=" << mState << ",sendActive=" << mSendActive << ",mRequestor.pending=" << mRequestor->pending() << ",receivedRequests.size=" << mReceivedRequests.size() << ",to=" << describe() << ")";
  if (mState < TLSProtocol::Connection::HANDSHAKE_DONE || mState > TLSProtocol::Connection::CONNECTED)
    return;
  if (mSendActive)
    return;
  if (!mScheduler->available())
    return;
  mSendActive = true;

  // we have to send in two stages, first a header of 8 bytes consiting of a message length and a message id
  auto entry = mScheduler->pop();
  MessageProperties properties = entry.properties;
  mMessageOutBody = entry.content;
  assert(mMessageOutBody);

  LOG(LOG_TAG, severity_level::verbose) << "TLSMessageProtocol::Connection::ensureSend outgoing message streamId=" << properties.messageId().streamId() << " (to " << describe() << ")";

  if (mMessageOutBody->size() >= MAX_SIZE_OF_MESSAGE) {
    std::ostringstream msg;
    msg << "Message queued to be sent is too large.  ("
        << "Size=" << mMessageOutBody->size() << ", Type="
        << DescribeMessageMagic(*mMessageOutBody)
        << ")";
    throw std::runtime_error(msg.str());
  }

  assert(mMessageOutBody->length() <= std::numeric_limits<MessageLength>::max());
  MessageHeader header(static_cast<MessageLength>(mMessageOutBody->length()), properties);
  mMessageOutHeader = header.encode();

  // after the message header is written, call the second stage to send the body
  boost::asio::async_write(*socket, boost::asio::buffer(&mMessageOutHeader, sizeof(mMessageOutHeader)),
                           boost::bind(&TLSMessageProtocol::Connection::boostOnHeaderSent, SharedFrom(*this),
                                       boost::asio::placeholders::error,
                                       boost::asio::placeholders::bytes_transferred));
}

void TLSMessageProtocol::Connection::handleSchedulerError(const MessageId& id, std::exception_ptr error) {
  assert(error != nullptr);

  severity_level severity;
  std::string action, caption, description;

  switch (id.type().value()) {
  case MessageType::REQUEST:
    severity = severity_level::error;
    action = "sending to";
    caption = "Unexpected exception";
    description = GetExceptionMessage(error);
    this->getProtocol()->mUncaughtReadExceptions++;
    break;

  case MessageType::RESPONSE:
    action = "handling";
    try {
      std::rethrow_exception(error);
    } catch (const RequestRefusedException& e) {
      severity = severity_level::warning;
      caption = "Refused";
      description = e.what();
    } catch (const Error& e) {
      severity = severity_level::warning;
      caption = "Error";
      description = e.what();
    } catch (...) {
      severity = severity_level::error;
      caption = "Stripping error details from reply";
      description = GetExceptionMessage(error);
      this->getProtocol()->mUncaughtReadExceptions++;
    }
    break;

  default:
    throw std::runtime_error("Unsupported message type " + std::to_string(static_cast<std::underlying_type_t<MessageType::Value>>(id.type().value())));
  }

  LOG(LOG_TAG, severity) << caption << " (" << action << " " << this->describe() << "): " << description;
}

void TLSMessageProtocol::Connection::start() {
  boost::asio::async_read(*socket,
    boost::asio::buffer(&mMessageInHeader, sizeof(mMessageInHeader)),
    boost::bind(&TLSMessageProtocol::Connection::boostOnHeaderReceived,
      SharedFrom(*this),
      boost::asio::placeholders::error,
      boost::asio::placeholders::bytes_transferred));

  // start keep-alive
  if (!mKeepAliveTimerRunning) {
    mKeepAliveTimerRunning = true;
    mKeepAliveTimer.expires_after(30s);
    mKeepAliveTimer.async_wait(boost::bind(&TLSMessageProtocol::Connection::boostOnKeepAliveTimerExpired, SharedFrom(*this), boost::asio::placeholders::error));
  }

  ensureSend();
}

void TLSMessageProtocol::Connection::boostOnHeaderSent(const boost::system::error_code& error, size_t /*bytes_transferred*/) {
  LOG(LOG_TAG, severity_level::verbose) << "boostOnHeaderSent (" << describe() << ")";
  if (error) {
    onConnectFailed(error);
    return;
  }

//   MessageLength dwLengthFromBody = messageOutBody == nullptr ? 0 : messageOutBody->length();
//   MessageLength dwLengthFromHeader = ntohl(*(MessageLength *)&messageOutHeader[0]) - sizeof(StreamId);
//
//   LOG(LOG_TAG, debug) << "Processing body (streamId=" << (htonl(*(StreamId *)(&messageOutHeader[sizeof(MessageLength)])) & ~TYPE_AND_FLAGS) << ",body=" << messageOutBody << ",len=" << dwLengthFromBody << ",header=" << dwLengthFromHeader << ")";
//
//   assert (dwLengthFromBody == dwLengthFromHeader);
  LOG(LOG_TAG, severity_level::verbose) << "Sending body (" << describe() << ")";

  if (!mMessageOutBody || mMessageOutBody->empty()) {
    boostOnMessageSent(error, 0);
    return;
  }

  boost::asio::async_write(*socket, boost::asio::buffer(mMessageOutBody->data(), mMessageOutBody->length()),
    boost::bind(&TLSMessageProtocol::Connection::boostOnMessageSent,
      SharedFrom(*this),
      boost::asio::placeholders::error,
      boost::asio::placeholders::bytes_transferred));

}

void TLSMessageProtocol::Connection::boostOnMessageSent(const boost::system::error_code& error, size_t /*bytes_transferred*/) {
  if (error) {
    onConnectFailed(error);
    return;
  }
  /* at this point, a message was successfully sent
   */

  LOG(LOG_TAG, severity_level::verbose) << "TLSMessageProtocol::Connection:boostOnMessageSent: "
                        << "completed sending message to " << describe();

  /* free body */
  mMessageOutBody.reset();
  mSendActive = false;

  mLastSend = decltype(mLastSend)::clock::now();

  ensureSend();
}

void TLSMessageProtocol::Connection::boostOnKeepAliveTimerExpired(const boost::system::error_code& error) {
  // timer cancelled
  if (error) {
    //std::cerr << "boostOnKeepAliveTimerExpired: " << describe() << " -> timer CANCELLED: " << error.value() << "/" << error.message() << std::endl;
    return;
  }

  mKeepAliveTimer.expires_after(30s);
  mKeepAliveTimer.async_wait(boost::bind(&TLSMessageProtocol::Connection::boostOnKeepAliveTimerExpired, SharedFrom(*this), boost::asio::placeholders::error));

  bool willSend = mLastSend <= decltype(mLastSend)::clock::now() - 30s;

  //std::cerr << "boostOnKeepAliveTimerExpired: " << describe() << " -> " << willSend << " (" << std::chrono::duration_cast<std::chrono::seconds>(decltype(mLastSend)::clock::now() - mLastSend) << " ago)" << std::endl;

  // if in the last 30 seconds something was sent, don't trigger the keep-alive
  if (!willSend)
    return;
  // if already active, do not interfere
  if (mSendActive)
    return;

  // mark active, so no interference can start
  mSendActive = true;

  // set empty message
  mMessageOutHeader = MessageHeader::MakeForControlMessage().encode();
  assert(mMessageOutBody == nullptr);

  // send async
  boost::asio::async_write(*socket, boost::asio::buffer(&mMessageOutHeader, sizeof(mMessageOutHeader)),
    boost::bind(&TLSMessageProtocol::Connection::boostOnMessageSent,
      SharedFrom(*this),
      boost::asio::placeholders::error,
      boost::asio::placeholders::bytes_transferred));
}

TLSMessageProtocol::Connection::Connection(std::shared_ptr<TLSMessageProtocol> protocol)
  : TLSProtocol::Connection(protocol), mMessageInBody(MAX_SIZE_OF_MESSAGE, '\0'), mKeepAliveTimer(*protocol->getIoContext()),
  mScheduler(Scheduler::Create(*protocol->getIoContext())), mRequestor(Requestor::Create(*protocol->getIoContext(), *mScheduler)) {
  mSchedulerAvailableSubscription = mScheduler->onAvailable.subscribe([this]() {this->ensureSend(); });
  mSchedulerExceptionSubscription = mScheduler->onError.subscribe([this](const MessageId& id, std::exception_ptr e) { this->handleSchedulerError(id, e); });
}

void TLSMessageProtocol::Connection::boostOnMessageReceived(const boost::system::error_code& error, size_t bytes) {
  if (error) {
    onConnectFailed(error);
    return;
  }

  try {
    auto header = MessageHeader::Decode(mMessageInHeader);
    assert(bytes == header.length());

    // Ensure that we keep receiving messages, even if an exception occurs
    PEP_DEFER(this->start());

    // make distinction between message types
    switch (header.properties().messageId().type().value()) {
    case MessageType::CONTROL:
      // No processing needed: just wait for the next message
      return;
    case MessageType::RESPONSE:
      this->handleReceivedResponse(mMessageInBody.c_str(), header);
      return;
    case MessageType::REQUEST:
      this->handleReceivedRequest(mMessageInBody.c_str(), header);
      return;
    }
    // default:
    LOG(LOG_TAG, pep::error) << "Failed to process message: unknown message type";
    this->onConnectFailed(boost::system::errc::make_error_code(boost::system::errc::errc_t::bad_message));

  } catch (...) {
    LOG(LOG_TAG, pep::error) << "Failed to process message: " << GetExceptionMessage(std::current_exception());
    this->onConnectFailed(boost::system::errc::make_error_code(boost::system::errc::errc_t::bad_message));
  }
}

void TLSMessageProtocol::Connection::logIncomingMessage(const std::string& type, const StreamId& streamId, const std::string& content) {
  assert(content.empty() || content.size() >= sizeof(MessageMagic));

  LOG(LOG_TAG, severity_level::verbose) << "Incoming " << type << " ("
    << (content.empty() ? std::string("without message magic") : DescribeMessageMagic(content))
    << ", stream id " << streamId << ", " << this->describe() << ")";
}

void TLSMessageProtocol::Connection::handleReceivedResponse(const char* content, const MessageHeader& header) {
  assert(header.properties().messageId().type() == MessageType::RESPONSE);

  auto body = std::string(content, header.length());
  auto streamId = header.properties().messageId().streamId();
  this->logIncomingMessage("response", streamId, body);

  mRequestor->processResponse(this->describe(), streamId, header.properties().flags(), std::move(body));
}

void TLSMessageProtocol::Connection::handleReceivedRequest(const char* content, const MessageHeader& header) {
  assert(header.properties().messageId().type() == MessageType::REQUEST);

  std::shared_ptr<std::string> abValue
    = std::make_shared<std::string>(content, header.length());
  auto streamId = header.properties().messageId().streamId();
  const auto& flags = header.properties().flags();
  this->logIncomingMessage("request", streamId, *abValue);

  auto it = mReceivedRequests.find(streamId);
  if (it != mReceivedRequests.end()) {
    // This is a follow-up chunk for a request whose head we received earlier: have the existing ReceivedRequest object handle it
    it->second.handleChunk(flags, abValue);
  }
  else {
    MessageSequence tail;
    if (flags.close()) {
      // An empty request with a close flag for an unknown stream can safely
      // be ignored, and is probably a superfluous close message, see #1188.
      if (abValue->size() == 0) {
        return;
      }
      // This is a (non-empty) request without followup messages
      tail = rxcpp::observable<>::empty<std::shared_ptr<std::string>>();
    }
    else {
      // This is (the head of) a request that has follow-up messages
      [[maybe_unused]] auto emplaced = mReceivedRequests.emplace(std::make_pair(streamId, ReceivedRequest())).second; // Create a ReceivedRequest object to (cache and) forward those follow-up chunks...
      assert(emplaced);
      tail = CreateObservable<std::shared_ptr<std::string>>([streamId, self = SharedFrom(*this)](rxcpp::subscriber<std::shared_ptr<std::string>> s) { // ... as soon as a subscriber wants them
        auto it = self->mReceivedRequests.find(streamId);
        if (it != self->mReceivedRequests.end()) {
          it->second.forwardTo(s);
        }
        else {
          // This code assumes that the tail observable will not be subscribed to after the observable returned by
          // handleXXXRequest has completed or resulted in an error. If you use 'tail' as part of the RX pipeline that will be returned
          // by handleXXXRequest, there should not be a problem.
          LOG(LOG_TAG, warning) << "Subscribed to the 'tail' observable when the incoming request has already been cleaned up";
          assert(false);
        }
      });
    }

    // Have request handled and enqueue return value as response messages
    auto responses = this->dispatchToHandler(abValue, tail)
      .observe_on(observe_on_asio(*getProtocol()->getIoContext()))
      .op(RxBeforeTermination([self = SharedFrom(*this), streamId](std::optional<std::exception_ptr>) {
      self->mReceivedRequests.erase(streamId);
        }));
    mScheduler->push(streamId, responses);
  }
}

void TLSMessageProtocol::Connection::ReceivedRequest::handleChunk(MessageFlags flags, std::shared_ptr<std::string> chunk) {
  if (flags.payload()) {
    if (mSubscriber) {
      mSubscriber->on_next(std::move(chunk));
    } else {
      mQueuedItems.emplace_back(std::move(chunk));
    }
  }
  if (flags.error()) {
    if (mSubscriber) {
      mSubscriber->on_error(nullptr);
    } else {
      mError = true;
    }
  } else if (flags.close()) {
    if (mSubscriber) {
      mSubscriber->on_completed();
    } else {
      mCompleted = true;
    }
  }
}

void TLSMessageProtocol::Connection::ReceivedRequest::forwardTo(rxcpp::subscriber<std::shared_ptr<std::string>> subscriber) {
  assert(mSubscriber == nullptr);

  mSubscriber = MakeSharedCopy(subscriber);
  // send items already queued
  for (auto& e : mQueuedItems) {
    mSubscriber->on_next(std::move(e));
  }
  mQueuedItems.clear();
  if (mError) {
    mSubscriber->on_error(nullptr);
  }
  else if (mCompleted) {
    mSubscriber->on_completed();
  }
}

MessageBatches TLSMessageProtocol::handleVersionRequest(std::shared_ptr<VersionRequest> request [[maybe_unused]]) {
  VersionResponse response{ BinaryVersion::current, ConfigVersion::Current() };
  MessageSequence retval = rxcpp::observable<>::from(std::make_shared<std::string>(Serialization::ToString(response)));
  return rxcpp::observable<>::from(retval);
}

MessageBatches TLSMessageProtocol::handlePingRequest(std::shared_ptr<PingRequest> request) {
  PingResponse resp(request->mId);
  MessageSequence retval = rxcpp::observable<>::from(std::make_shared<std::string>(Serialization::ToString(resp)));
  return rxcpp::observable<>::from(retval);
}

TLSMessageProtocol::TLSMessageProtocol(std::shared_ptr<Parameters> parameters)
  : TLSProtocol(parameters) {
  this->registerHousekeepingRequestHandler(&TLSMessageProtocol::handlePingRequest);
  this->registerHousekeepingRequestHandler(&TLSMessageProtocol::handleVersionRequest);
}

WaitGroup::Action TLSMessageProtocol::Connection::pendVersionVerification() {
  mVersionCorrect = false;
  mVersionVerification = WaitGroup::Create();
  return mVersionVerification->add("version verification");
}

MessageBatches TLSMessageProtocol::Connection::dispatchToHandler(
    std::shared_ptr<std::string> request, MessageSequence tail) {
  try {
    auto magic = PopMessageMagic(*request);

    // Housekeeping requests are always handled
    if (this->getProtocol()->mHousekeepingRequests.find(magic) != this->getProtocol()->mHousekeepingRequests.cend()) {
      return this->getProtocol()->handleRequest(magic, request, tail);
    }

    // Other request handlers are invoked after we've made sure that we're dealing with a party that has the correct version
    return mVersionVerification->delayObservable<MessageSequence>([weak = pep::static_pointer_cast<Connection>(weak_from_this()), magic, request, tail]()->MessageBatches {
      try {
        auto self = weak.lock();
        if (!self) {
          throw std::runtime_error("Connection closed before request could be handled");
        }
        if (!self->mVersionCorrect) {
          throw RequestRefusedException("Refusing to handle request from connected party with incompatible network protocol version");
        }
        return self->getProtocol()->handleRequest(magic, request, tail);
      }
      catch (...) {
        return rxcpp::observable<>::error<MessageSequence>(std::current_exception());
      }
    });
  } catch (...) {
    return rxcpp::observable<>::error<MessageSequence>(std::current_exception());
  }
}

rxcpp::observable<std::string> TLSMessageProtocol::Connection::sendRequest(std::shared_ptr<std::string> message, std::optional<MessageBatches> tail) {
  assert(message);
  // This is a redundant check, such that the caller will receive an exception
  // with a better stack trace.
  if (message->size() >= MAX_SIZE_OF_MESSAGE) {
    std::ostringstream msg;
    msg << "Message (" << DescribeMessageMagic(*message)
        << ") to " << describe() << " is too large ("
        << message->length() << ")";
    throw std::runtime_error(msg.str());
  }

  LOG(LOG_TAG, severity_level::verbose)
    << "TLSMessageProtocol::Connection::sendRequest: sending "
    << DescribeMessageMagic(*message)
    << " of size " << message->length() << " to " << describe();
  assert(message->size() != 0U);

  return mRequestor->send(message, tail);
}

/*virtual*/ void TLSMessageProtocol::Connection::onConnectFailed(const boost::system::error_code& error) {
  if (this->mState != TLSProtocol::Connection::SHUTDOWN) {
    if (error == boost::asio::error::eof) {
      // Peer shut the connection down on purpose and is probably waiting for
      // us to do the same.  We will, but don't care about the success of the
      // shutdown.
      socket->async_shutdown([captureSocket = socket](
        boost::system::error_code error) {});
    }
    else {
      LOG(LOG_TAG, severity_level::warning)
        << "TLSMessageProtocol::Connection::onConnectFailed ("
        << error << ") with "
        << describe();
    }
  }

  // Cancel sending of previously scheduled request and response messages
  mScheduler->clear();
  // Stop sending keepalive messages
  mKeepAliveTimer.cancel();
  mKeepAliveTimerRunning = false;
  // Clear state for outgoing messages
  mSendActive = false;
  mMessageOutBody.reset();

  // Let base class clean up for itself
  TLSProtocol::Connection::onConnectFailed(error);

  // Discard pending requests that cannot be re-sent
  mRequestor->purge();
}

bool TLSMessageProtocol::Connection::resendOutstandingRequests() {
  if (mState == TLSProtocol::Connection::SHUTDOWN) {
    return false;
  }

  if (mScheduler->available()) {
    throw std::runtime_error("Pending requests can only be re-sent when there are no outgoing messages"); // I.e. invoke only after onConnectFailed, which clears the mScheduler
  }

  mRequestor->resend();

  return true;
}

bool TLSMessageProtocol::IncompatibleRemote::operator<(const IncompatibleRemote& rhs) const {
  return std::tie(this->address, this->binary, this->config) < std::tie(rhs.address, rhs.binary, rhs.config);
}

void TLSMessageProtocol::handleVersionResponse(const boost::asio::ip::address& address, const VersionResponse& response, const std::string& description) {
  if (BinaryVersion::current.getProtocolChecksum() != response.binary.getProtocolChecksum()) {
    auto refuse = response.binary.isGitlabBuild() && BinaryVersion::current.isGitlabBuild(); // TODO: perhaps make this depend on ConfigVersion::getReference() == "local"?

    std::string msg;
    severity_level severity;
    if (refuse) {
      msg = "Refusing";
      severity = error;
    }
    else {
      msg = "Development genuflection: allowing";
      severity = warning;
    }
    msg += " connection between incompatible remote " + description
      + " (" + response.binary.getProtocolChecksum() + " at " + address.to_string()
      + ") and local (" + BinaryVersion::current.getProtocolChecksum() + ") software versions";

    IncompatibleRemote remote{ address, GetIncompatibleVersionSummary(response.binary), GetIncompatibleVersionSummary(response.config) };
    if (mIncompatibleRemotes.emplace(std::move(remote)).second) { // If we haven't seen this incompatible remote before, log about it now (once)
      LOG(LOG_TAG, severity) << msg;
      LogIncompatibleVersionDetails(severity, "binary", response.binary, BinaryVersion::current);
      LogIncompatibleVersionDetails(severity, "config", response.config, ConfigVersion::Current());
    }

    if (refuse) {
      throw ConnectionFailureException(boost::system::errc::wrong_protocol_type, msg);
    }
  }
}

/*virtual*/ void TLSMessageProtocol::Connection::onHandshakeSuccess() {
  auto self = SharedFrom(*this);

  auto verification = pendVersionVerification();
  boost::asio::io_context& io_context = *getProtocol()->getIoContext();
  RxEnsureProgress(io_context,
    "Version verification for " + this->describe(),
    sendRequest<VersionResponse>(VersionRequest()))
    .observe_on(observe_on_asio(io_context))
    .subscribe(
      [self](VersionResponse response) {
        self->getProtocol()->handleVersionResponse(self->getSocket().remote_endpoint().address(), response, self->describe());
      },
      [self, verification](std::exception_ptr ep) {
        PEP_DEFER(verification.done());
        auto reason = boost::system::errc::bad_message;
        try {
          std::rethrow_exception(ep);
        }
        catch (const ConnectionFailureException& e) {
          reason = e.getReason();
        }
        catch (...) {
          /*Ignore*/
          // Keep default reason = boost::system::errc::bad_message;
        }
        self->onConnectFailed(make_error_code(reason));
      },
      [self, verification]() {
        PEP_DEFER(verification.done());
        self->mVersionCorrect = true;
        self->onConnectSuccess();
      });

  // Start accepting messages now to allow connected party to retrieve our version
  start();
}

unsigned int TLSMessageProtocol::getNumberOfUncaughtReadExceptions() const noexcept {
  return mUncaughtReadExceptions;
}

}
