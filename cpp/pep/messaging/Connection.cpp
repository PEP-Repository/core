#include <pep/async/OnAsio.hpp>
#include <pep/async/RxBeforeTermination.hpp>
#include <pep/messaging/Node.hpp>
#include <pep/messaging/ConnectionFailureException.hpp>
#include <pep/messaging/MessagingSerializers.hpp>
#include <pep/utils/Defer.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/Shared.hpp>

#include <boost/bind/bind.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/asio/ssl/error.hpp>

namespace pep::messaging {

namespace {

const std::string LOG_TAG = "Messaging connection";

class RequestRefusedException : public Error {
public:
  explicit inline RequestRefusedException(const std::string& reason) : Error(reason) {}
};

}

void Connection::handleHeaderReceived(const networking::SizedTransfer::Result& result) {
  if (!this->prepareBodyTransfer(result)) {
    LOG(LOG_TAG, severity_level::verbose)
      << " \\__ error! " << error << ", that is, " << GetExceptionMessage(result.exception());
    return;
  }

  try {
    assert(*result == sizeof(mMessageInHeader));
    auto header = MessageHeader::Decode(mMessageInHeader);
    auto length = header.length();

    if (length > MAX_SIZE_OF_MESSAGE) {
      LOG(LOG_TAG, severity_level::error)
        << "Connection::handleHeaderReceived: "
        << "refusing " << length << "-byte message from " << describe()
        << " because it's larger than the maximum of " << MAX_SIZE_OF_MESSAGE << " bytes";
      return this->handleError(std::make_exception_ptr(boost::system::system_error(boost::asio::error::message_size)));
    }

    LOG(LOG_TAG, severity_level::verbose)
      << "Connection::handleHeaderReceived: "
      << "receiving " << length << "-byte message from " << describe();

    if (length == 0U) {
      this->handleMessageReceived(networking::SizedTransfer::Result::Success(0U));
      return;
    }

    mBinary->asyncRead(mMessageInBody.data(), length, [self = SharedFrom(*this)](const networking::SizedTransfer::Result& result) {
      self->handleMessageReceived(result);
      });
  }
  catch (...) {
    LOG(LOG_TAG, pep::error) << "Failed to process message header: " << GetExceptionMessage(std::current_exception());
    this->handleError(std::make_exception_ptr(boost::system::system_error(boost::system::errc::make_error_code(boost::system::errc::errc_t::bad_message))));
  }
}

void Connection::ensureSend() {
  LOG(LOG_TAG, severity_level::verbose) << "Connection::ensureSend (sendActive=" << mSendActive << ",mRequestor.pending=" << mRequestor->pending() << ",receivedRequests.size=" << mIncomingRequestTails.size() << ",to=" << describe() << ")";
  if (!this->isConnected()) {
    return;
  }
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

  LOG(LOG_TAG, severity_level::verbose) << "Connection::ensureSend outgoing message streamId=" << properties.messageId().streamId() << " (to " << describe() << ")";

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
  mBinary->asyncWrite(&mMessageOutHeader, sizeof(mMessageOutHeader), [self = SharedFrom(*this)](const networking::SizedTransfer::Result& result) {
    self->handleHeaderSent(result);
    });
}

void Connection::handleSchedulerError(const MessageId& id, std::exception_ptr error) {
  assert(error != nullptr);

  severity_level severity;
  std::string action, caption, description;

  switch (id.type().value()) {
  case MessageType::REQUEST:
    severity = severity_level::error;
    action = "sending to";
    caption = "Unexpected exception";
    description = GetExceptionMessage(error);
    onUncaughtReadException.notify(error);
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
      onUncaughtReadException.notify(error);
    }
    break;

  default:
    throw std::runtime_error("Unsupported message type " + std::to_string(ToUnderlying(id.type().value())));
  }

  LOG(LOG_TAG, severity) << caption << " (" << action << " " << this->describe() << "): " << description;
}

void Connection::start() {
  if (this->isConnected()) {
    mBinary->asyncRead(&mMessageInHeader, sizeof(mMessageInHeader), [self = SharedFrom(*this)](const networking::SizedTransfer::Result& result) {
      self->handleHeaderReceived(result);
      });

    // start keep-alive
    if (!mKeepAliveTimerRunning) {
      mKeepAliveTimerRunning = true;
      mKeepAliveTimer.expires_after(std::chrono::seconds(30));
      mKeepAliveTimer.async_wait(boost::bind(&Connection::handleKeepAliveTimerExpired, SharedFrom(*this), boost::asio::placeholders::error));
    }

    ensureSend();
  }
}

void Connection::handleHeaderSent(const networking::SizedTransfer::Result& result) {
  LOG(LOG_TAG, severity_level::verbose) << "handleHeaderSent (" << describe() << ")";
  if (!this->prepareBodyTransfer(result)) {
    return;
  }

  LOG(LOG_TAG, severity_level::verbose) << "Sending body (" << describe() << ")";

  if (!mMessageOutBody || mMessageOutBody->empty()) {
    handleMessageSent(networking::SizedTransfer::Result::Success(0));
    return;
  }

  mBinary->asyncWrite(mMessageOutBody->data(), mMessageOutBody->length(), [self = SharedFrom(*this)](const networking::SizedTransfer::Result& result) {
    self->handleMessageSent(result);
    });
}

void Connection::handleMessageSent(const networking::SizedTransfer::Result& result) {
  if (!result) {
    handleError(result.exception());
    return;
  }
  /* at this point, a message was successfully sent
   */

  LOG(LOG_TAG, severity_level::verbose) << "Connection:handleMessageSent: "
    << "completed sending message to " << describe();

  /* free body */
  mMessageOutBody.reset();
  mSendActive = false;

  mLastSend = std::chrono::steady_clock::now();

  ensureSend();
}

void Connection::handleKeepAliveTimerExpired(const boost::system::error_code& error) {
  // timer cancelled
  if (error) {
    return;
  }
  // Don't (re)start the timer if the connection isn't fully established (probably reinitializing or finalizing)
  if (this->status() != Status::initialized) {
    return;
  }

  mKeepAliveTimer.expires_after(std::chrono::seconds(30));
  mKeepAliveTimer.async_wait(boost::bind(&Connection::handleKeepAliveTimerExpired, SharedFrom(*this), boost::asio::placeholders::error));

  bool willSend = mLastSend <= std::chrono::steady_clock::now() - std::chrono::seconds(30);

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
  mBinary->asyncWrite(&mMessageOutHeader, sizeof(mMessageOutHeader), [self = SharedFrom(*this)](const networking::SizedTransfer::Result& result) {
    self->handleMessageSent(result);
    });
}

Connection::Connection(std::shared_ptr<Node> node, std::shared_ptr<networking::Connection> binary, boost::asio::io_context& ioContext, RequestHandler* requestHandler)
  : mMessageInBody(MAX_SIZE_OF_MESSAGE, '\0'), mKeepAliveTimer(ioContext), mScheduler(Scheduler::Create(ioContext)), mRequestor(Requestor::Create(ioContext, *mScheduler)),
  mNode(node), mDescription(node->describe()), mBinary(std::move(binary)), mIoContext(ioContext), mRequestHandler(requestHandler) {
  assert(mBinary->status() == networking::Transport::ConnectivityStatus::connected);
  assert(mNode.lock() != nullptr);

  this->setStatus(Status::initializing);

  mSchedulerAvailableSubscription = mScheduler->onAvailable.subscribe([this]() {this->ensureSend(); });
  mSchedulerExceptionSubscription = mScheduler->onError.subscribe([this](const MessageId& id, std::exception_ptr e) { this->handleSchedulerError(id, e); });
}

void Connection::handleMessageReceived(const networking::SizedTransfer::Result& result) {
  if (!result) {
    handleError(result.exception());
    return;
  }

  try {
    auto header = MessageHeader::Decode(mMessageInHeader);
    assert(*result == header.length());
    const auto& messageId = header.properties().messageId();

    // Ensure that we keep receiving messages, even if an exception occurs
    PEP_DEFER(this->start());

    // make distinction between message types
    switch (messageId.type().value()) {
    case MessageType::CONTROL:
      // No processing needed: just wait for the next message
      return;

    case MessageType::RESPONSE:
      this->processReceivedResponse(messageId.streamId(), header.properties().flags(), this->getReceivedMessageContent(header));
      return;
    case MessageType::REQUEST:
      this->processReceivedRequest(messageId.streamId(), header.properties().flags(), this->getReceivedMessageContent(header));
      return;
    }
  }
  catch (...) {
    LOG(LOG_TAG, pep::error) << "Failed to process message: " << GetExceptionMessage(std::current_exception());
    // Processed by generic "bad message" handling outside the "catch" clause
  }

  this->handleError(std::make_exception_ptr(boost::system::system_error(boost::system::errc::make_error_code(boost::system::errc::errc_t::bad_message))));
}

std::string Connection::getReceivedMessageContent(const MessageHeader& header) {
  const auto& messageId = header.properties().messageId();

  auto result = mMessageInBody.substr(0U, header.length());
  assert(result.empty() || result.size() >= sizeof(MessageMagic));

  LOG(LOG_TAG, severity_level::verbose) << "Incoming " << messageId.type().describe() << " ("
    << (result.empty() ? std::string("without message magic") : DescribeMessageMagic(result))
    << ", stream id " << messageId.streamId() << ", " << this->describe() << ")";

  return result;
}

void Connection::close() {
  mBinaryStatusSubscription.cancel();
  mBinary.reset();
  this->clearState();
  this->setStatus(Status::finalizing);
}

void Connection::processReceivedResponse(const StreamId& streamId, const Flags& flags, std::string content) {
  mRequestor->processResponse(this->describe(), streamId, flags, std::move(content));
}

void Connection::processReceivedRequest(const StreamId& streamId, const Flags& flags, std::string content) {
  std::shared_ptr<std::string> abValue
    = std::make_shared<std::string>(std::move(content));

  auto it = mIncomingRequestTails.find(streamId);
  if (it != mIncomingRequestTails.end()) {
    // This is a follow-up chunk for a request whose head we received earlier: have the existing IncomingRequestTail object handle it
    it->second.handleChunk(flags, abValue);
  }
  else if (mScheduler->hasPendingResponseFor(streamId)) {
    // See https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2627
    std::string detail;
    if (abValue->size() >= sizeof(MessageMagic)) {
      detail = DescribeMessageMagic(*abValue);
    }
    else {
      detail = std::to_string(abValue->size()) + "-byte";
    }
    LOG(LOG_TAG, info) << "Dropping (followup?) " << detail << " message for request stream " << streamId.value() << ", which we're already replying to";
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
      [[maybe_unused]] auto emplaced = mIncomingRequestTails.emplace(std::make_pair(streamId, IncomingRequestTail())).second; // Create an IncomingRequestTail object to (cache and) forward those follow-up chunks...
      assert(emplaced);
      tail = CreateObservable<std::shared_ptr<std::string>>([streamId, self = SharedFrom(*this)](rxcpp::subscriber<std::shared_ptr<std::string>> s) { // ... as soon as a subscriber wants them
        auto it = self->mIncomingRequestTails.find(streamId);
        if (it != self->mIncomingRequestTails.end()) {
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
    this->dispatchRequest(streamId, abValue, tail);
  }
}

void Connection::IncomingRequestTail::handleChunk(const Flags& flags, std::shared_ptr<std::string> chunk) {
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

MessageBatches Connection::handleVersionRequest(std::shared_ptr<std::string> request [[maybe_unused]], MessageSequence chunks [[maybe_unused]] ) {
  VersionResponse response{ BinaryVersion::current, ConfigVersion::Current() };
  MessageSequence retval = rxcpp::observable<>::from(std::make_shared<std::string>(Serialization::ToString(response)));
  return rxcpp::observable<>::from(retval);
}

void Connection::dispatchRequest(
  const StreamId& streamId, std::shared_ptr<std::string> request, MessageSequence chunks) {
  std::optional<MessageBatches> responses;

  pep::MessageMagic magic = 0U;
  try {
    magic = PopMessageMagic(*request);

    if (magic == MessageMagician<VersionRequest>::GetMagic()) {
      responses = this->handleVersionRequest(request, chunks);
    }
    else if (mRequestHandler == nullptr) {
      this->handleError(std::make_exception_ptr(std::runtime_error("No request handler present")));
    }
    else if (!mVersionValidated) {
      if (std::any_of(mPrematureRequests.begin(), mPrematureRequests.end(), [&streamId](const PrematureRequest& candidate) {
        return candidate.streamId == streamId;
        })) {
        throw std::runtime_error("Received multiple premature requests with stream ID " + std::to_string(streamId.value()));
      }
      mPrematureRequests.emplace_back(PrematureRequest({ streamId, magic, request, chunks }));
    }
    else {
      responses = mRequestHandler->handleRequest(magic, request, chunks);
    }
  } catch (...) {
    responses = rxcpp::observable<>::error<MessageSequence>(std::current_exception());
  }

  if (responses.has_value()) {
    try {
      this->scheduleResponses(streamId, *responses);
    }
    catch (...) {
      LOG(LOG_TAG, error) << "Error scheduling response(s) for received " << DescribeMessageMagic(magic) << " request: " << GetExceptionMessage(std::current_exception())
        << '\n' << "    Connection status is " << ToUnderlying(this->status()) << "; scheduler.available is " << std::boolalpha << mScheduler->available();
      throw;
    }
  }
}

void Connection::scheduleResponses(const StreamId& streamId, MessageBatches responses) {
  mScheduler->push(streamId, responses
    .observe_on(observe_on_asio(mIoContext))
    .op(RxBeforeTermination([self = SharedFrom(*this), streamId](std::optional<std::exception_ptr>) {
      self->mIncomingRequestTails.erase(streamId);
      })));
}

rxcpp::observable<std::string> Connection::sendRequest(std::shared_ptr<std::string> message, std::optional<MessageBatches> tail) {
  return this->sendRequest(message, tail, false);
}

rxcpp::observable<std::string> Connection::sendRequest(std::shared_ptr<std::string> message, std::optional<MessageBatches> tail, bool isVersionCheck) {
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
    << "Connection::sendRequest: sending "
    << DescribeMessageMagic(*message)
    << " of size " << message->length() << " to " << describe();
  assert(message->size() != 0U);

  return mRequestor->send(message, tail, isVersionCheck || mVersionValidated, !isVersionCheck);
}

bool Connection::prepareBodyTransfer(const networking::SizedTransfer::Result& headerResult) {
  if (!headerResult) {
    this->handleError(headerResult.exception());
    return false;
  }
  if (!this->isConnected()) {
    this->handleError(std::make_exception_ptr(boost::system::system_error(boost::asio::error::connection_aborted)));
    return false;
  }
  return true;
}

void Connection::handleError(std::exception_ptr exception) {
  auto shouldLog = [](std::exception_ptr e) {
    try {
      std::rethrow_exception(e);
    } catch (const boost::system::system_error& boost) {
      // TODO: decouple from Boost; consolidate duplicate code with IsSpecificAsioError
      std::set<boost::system::error_code> suppress{
        boost::asio::error::make_error_code(boost::asio::error::connection_aborted),
        boost::asio::error::make_error_code(boost::asio::error::connection_reset),
        boost::asio::error::make_error_code(boost::asio::error::operation_aborted),
        boost::asio::error::make_error_code(boost::asio::error::eof),
        boost::asio::ssl::error::make_error_code(boost::asio::ssl::error::stream_errors::stream_truncated)
      };
      return !suppress.contains(boost.code());
    } catch (...) {
      return true;
    }
  };

  if (shouldLog(exception)) {
    LOG(LOG_TAG, severity_level::warning)
      << "Error with " << this->describe()
      << ": " << GetExceptionMessage(exception);
  }

  if (mBinary != nullptr) {
    mBinary->close();
  }
}

void Connection::clearState() {
  // Let request handlers know that they won't receive further tail segments
  for (auto& incoming : mIncomingRequestTails) {
    incoming.second.abort();
  }

  // Cancel sending of previously scheduled request and response messages
  mScheduler->clear();
  // Stop sending keepalive messages 
  mKeepAliveTimer.cancel();
  mKeepAliveTimerRunning = false;
  // Clear state for outgoing messages
  mSendActive = false;
  mMessageOutBody.reset();
  // Clear state for incoming messages
  mVersionValidated = false;

  // Discard cached incoming requests
  mPrematureRequests.clear();
  // Discard pending requests that cannot be re-sent
  mRequestor->purge();
}

void Connection::handleBinaryConnectionEstablished(Attempt::Handler notify) {
  auto self = SharedFrom(*this);

  assert(!mVersionValidated);
  this->sendRequest(MakeSharedCopy(Serialization::ToString(VersionRequest())), std::nullopt, true)
    .map([](std::string response) {return Serialization::FromString<VersionResponse>(response); })
    .observe_on(observe_on_asio(mIoContext))
    .subscribe(
      [self](VersionResponse response) {
        self->handleVersionResponse(response);
      },
      [self, notify](std::exception_ptr ep) {
        LOG(LOG_TAG, warning) << "Version check failed: " << GetExceptionMessage(ep);
        auto getReason = [](std::exception_ptr exception) {
          try {
            std::rethrow_exception(exception);
          } catch (const ConnectionFailureException& e) {
            return e.getReason();
          } catch (...) {
            return boost::system::errc::bad_message;
          }
        };
        auto reason = getReason(ep);
        auto error = std::make_exception_ptr(boost::system::system_error(make_error_code(reason)));
        self->handleError(error);
        notify(Attempt::Result::Failure(error));
      },
      [self, notify]() {
        if (!self->mVersionValidated) {
          auto error = std::make_exception_ptr(ConnectionFailureException::ForVersionCheckFailure("No version response received"));
          self->handleError(error);
          notify(Attempt::Result::Failure(error));
        }
        else {
          notify(Attempt::Result::Success(self));
        }
      });

  // Start accepting messages now to allow connected party to retrieve our version
  start();
}

void Connection::handleVersionResponse(const VersionResponse& response) {
  assert(!mVersionValidated);

  auto node = mNode.lock();
  if (node == nullptr) {
    throw ConnectionFailureException(boost::system::errc::owner_dead, "Node was discarded before connection could perform version verification");
  }
  if (this->status() != Status::initializing) {
    throw ConnectionFailureException(boost::system::errc::connection_aborted, "Connection was closed before it could perform version verification");
  }

  assert(mBinary != nullptr);
  node->vetConnectionWith(this->describe(), mBinary->remoteAddress(), response.binary, response.config); // Raises an exception if connection should be refused

  mVersionValidated = true;
  this->setStatus(Status::initialized);

  // Schedule (re)sendable requests
  mRequestor->resend();

  // Handle requests that were received before version check completed (if any)
  auto premature = std::move(mPrematureRequests);
  assert(mPrematureRequests.empty()); // Move constructor should clear the moved-from vector: see https://stackoverflow.com/a/17735913
  for (auto& request : premature) {
    assert(mRequestHandler != nullptr); // Otherwise premature requests wouldn't have been stored
    try {
      this->scheduleResponses(request.streamId, mRequestHandler->handleRequest(request.magic, request.head, request.tail));
    }
    catch (...) {
      LOG(LOG_TAG, error) << "Error scheduling response(s) for premature " << DescribeMessageMagic(request.magic) << " request: " << GetExceptionMessage(std::current_exception());
      throw;
    }
  }
}

void Connection::handleBinaryConnectivityChange(const networking::Connection::ConnectivityChange& change) {
  switch (change.updated) {
  case networking::Transport::ConnectivityStatus::unconnected: // Prevent compiler warnings due to switch statement not handling all enum values
    assert(false);
    return;
  case networking::Transport::ConnectivityStatus::reconnecting:
    this->clearState();
    this->setStatus(Status::reinitializing);
    return;
  case networking::Transport::ConnectivityStatus::connecting:
    assert(!mVersionValidated);
    this->setStatus(Status::initializing);
    return;
  case networking::Transport::ConnectivityStatus::connected:
    this->handleBinaryConnectionEstablished([](const auto&) { /* ignore: "handle" method updates state */});
    return;
  case networking::Transport::ConnectivityStatus::disconnecting:
  case networking::Transport::ConnectivityStatus::disconnected:
    this->close();
    return;
  }

  throw std::runtime_error("Unsupported binary connection status " + std::to_string(ToUnderlying(change.updated)));
}

std::string Connection::describe() const {
  return mDescription;
}

bool Connection::isConnected() const noexcept {
  return mBinary != nullptr && mBinary->isConnected();
}

void Connection::Open(std::shared_ptr<Node> node, std::shared_ptr<networking::Connection> binary, boost::asio::io_context& ioContext, RequestHandler* requestHandler, Attempt::Handler notify) {
  // Create a Connection so it can perform its version check
  assert(binary->isConnected());
  auto instance = std::shared_ptr<Connection>(new Connection(node, binary, ioContext, requestHandler));
  assert(instance->status() == Status::initializing);

  // Subscribe the Connection to connectivity changes in the networking::Connection: the constructor couldn't do so because it can't get a shared_ptr to itself
  instance->mBinaryStatusSubscription = instance->mBinary->onConnectivityChange.subscribe([weak = std::weak_ptr<Connection>(instance)](const networking::Connection::ConnectivityChange& change) {
    auto self = weak.lock();
    if (self != nullptr) {
      self->handleBinaryConnectivityChange(change);
    }
    });

  // Have the instance initialize itself, invoking the handler when done
  instance->handleBinaryConnectionEstablished(std::move(notify));
}

void Connection::IncomingRequestTail::forwardTo(rxcpp::subscriber<std::shared_ptr<std::string>> subscriber) {
  assert(mSubscriber == nullptr);

  mSubscriber = MakeSharedCopy(subscriber);
  // send items already queued
  for (auto& e : mQueuedItems) {
    mSubscriber->on_next(std::move(e));
  }
  mQueuedItems.clear();
  if (mError) {
    mSubscriber->on_error(nullptr);
  } else if (mCompleted) {
    mSubscriber->on_completed();
  }
}

void Connection::IncomingRequestTail::abort() {
  this->handleChunk(Flags::MakeError(), pep::MakeSharedCopy(std::string()));
}

}
