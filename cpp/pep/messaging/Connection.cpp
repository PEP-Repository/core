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

const std::string LogTag = "Messaging connection";

class RequestRefusedException : public Error {
public:
  explicit inline RequestRefusedException(const std::string& reason) : Error(reason) {}
};

}

void Connection::handleHeaderReceived(const networking::SizedTransfer::Result& result) {
  if (!this->prepareBodyTransfer(result)) {
    PEP_LOG(LogTag, Severity::Verbose)
      << " \\__ error: " << GetExceptionMessage(result.exception());
    return;
  }

  try {
    assert(*result == sizeof(messageInHeader_));
    auto header = MessageHeader::Decode(messageInHeader_);
    auto length = header.length();

    if (length > MaxSizeOfMessage) {
      PEP_LOG(LogTag, Severity::Warning)
        << "Connection::handleHeaderReceived: "
        << "refusing " << length << "-byte message from " << describe()
        << " because it's larger than the maximum of " << MaxSizeOfMessage << " bytes";
      return this->handleError(std::make_exception_ptr(boost::system::system_error(boost::asio::error::message_size)));
    }

    PEP_LOG(LogTag, Severity::Verbose)
      << "Connection::handleHeaderReceived: "
      << "receiving " << length << "-byte message from " << describe();

    if (length == 0U) {
      this->handleMessageReceived(networking::SizedTransfer::Result::Success(0U));
      return;
    }

    binary_->asyncRead(messageInBody_.data(), length, [self = SharedFrom(*this)](const networking::SizedTransfer::Result& result) {
      self->handleMessageReceived(result);
      });
  }
  catch (...) {
    PEP_LOG(LogTag, Severity::Warning) << "Failed to process message header: " << GetExceptionMessage(std::current_exception());
    this->handleError(std::make_exception_ptr(boost::system::system_error(boost::system::errc::make_error_code(boost::system::errc::errc_t::bad_message))));
  }
}

void Connection::ensureSend() {
  PEP_LOG(LogTag, Severity::Verbose) << "Connection::ensureSend (sendActive=" << sendActive_ << ",requestor_.pending=" << requestor_->pending() << ",receivedRequests.size=" << incomingRequestTails_.size() << ",to=" << describe() << ")";
  if (!this->isConnected()) {
    return;
  }
  if (sendActive_)
    return;
  if (!scheduler_->available())
    return;
  sendActive_ = true;

  // we have to send in two stages, first a header of 8 bytes consiting of a message length and a message id
  auto entry = scheduler_->pop();
  MessageProperties properties = entry.properties;
  messageOutBody_ = entry.content;
  assert(messageOutBody_);

  PEP_LOG(LogTag, Severity::Verbose) << "Connection::ensureSend outgoing message streamId=" << properties.messageId().streamId() << " (to " << describe() << ")";

  if (messageOutBody_->size() >= MaxSizeOfMessage) {
    std::ostringstream msg;
    msg << "Message queued to be sent is too large.  ("
      << "Size=" << messageOutBody_->size() << ", Type="
      << DescribeMessageMagic(*messageOutBody_)
      << ")";
    throw std::runtime_error(msg.str());
  }

  assert(messageOutBody_->length() <= std::numeric_limits<MessageLength>::max());
  MessageHeader header(static_cast<MessageLength>(messageOutBody_->length()), properties);
  messageOutHeader_ = header.encode();

  // after the message header is written, call the second stage to send the body
  binary_->asyncWrite(&messageOutHeader_, sizeof(messageOutHeader_), [self = SharedFrom(*this)](const networking::SizedTransfer::Result& result) {
    self->handleHeaderSent(result);
    });
}

void Connection::handleSchedulerError(const MessageId& id, std::exception_ptr error) {
  assert(error != nullptr);

  Severity severity{};
  std::string action, caption, description;

  switch (id.type().value()) {
  case MessageType::Request:
    severity = Severity::Error;
    action = "sending to";
    caption = "Unexpected exception";
    description = GetExceptionMessage(error);
    onUncaughtReadException.notify(error);
    break;

  case MessageType::Response:
    action = "handling";
    try {
      std::rethrow_exception(error);
    } catch (const RequestRefusedException& e) {
      severity = Severity::Warning;
      caption = "Refused";
      description = e.what();
    } catch (const Error& e) {
      severity = Severity::Warning;
      caption = "Error";
      description = e.what();
    } catch (...) {
      severity = Severity::Error;
      caption = "Unexpected exception, treating as uncaught and stripping error details from reply";
      description = GetExceptionMessage(error);
      onUncaughtReadException.notify(error);
    }
    break;

  default:
    throw std::runtime_error("Unsupported message type " + std::to_string(ToUnderlying(id.type().value())));
  }

  PEP_LOG(LogTag, severity) << caption << " (" << action << " " << this->describe() << "): " << description;
}

void Connection::start() {
  if (this->isConnected()) {
    binary_->asyncRead(&messageInHeader_, sizeof(messageInHeader_), [self = SharedFrom(*this)](const networking::SizedTransfer::Result& result) {
      self->handleHeaderReceived(result);
      });

    // start keep-alive
    if (!keepAliveTimerRunning_) {
      keepAliveTimerRunning_ = true;
      keepAliveTimer_.expires_after(std::chrono::seconds(30));
      keepAliveTimer_.async_wait(boost::bind(&Connection::handleKeepAliveTimerExpired, SharedFrom(*this), boost::asio::placeholders::error));
    }

    ensureSend();
  }
}

void Connection::handleHeaderSent(const networking::SizedTransfer::Result& result) {
  PEP_LOG(LogTag, Severity::Verbose) << "handleHeaderSent (" << describe() << ")";
  if (!this->prepareBodyTransfer(result)) {
    return;
  }

  PEP_LOG(LogTag, Severity::Verbose) << "Sending body (" << describe() << ")";

  if (!messageOutBody_ || messageOutBody_->empty()) {
    handleMessageSent(networking::SizedTransfer::Result::Success(0));
    return;
  }

  binary_->asyncWrite(messageOutBody_->data(), messageOutBody_->length(), [self = SharedFrom(*this)](const networking::SizedTransfer::Result& result) {
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

  PEP_LOG(LogTag, Severity::Verbose) << "Connection:handleMessageSent: "
    << "completed sending message to " << describe();

  /* free body */
  messageOutBody_.reset();
  sendActive_ = false;

  lastSend_ = std::chrono::steady_clock::now();

  ensureSend();
}

void Connection::handleKeepAliveTimerExpired(const boost::system::error_code& error) {
  // timer cancelled
  if (error) {
    return;
  }
  // Don't (re)start the timer if the connection isn't fully established (probably reinitializing or finalizing)
  if (this->status() != Status::Initialized) {
    return;
  }

  keepAliveTimer_.expires_after(std::chrono::seconds(30));
  keepAliveTimer_.async_wait(boost::bind(&Connection::handleKeepAliveTimerExpired, SharedFrom(*this), boost::asio::placeholders::error));

  bool willSend = lastSend_ <= std::chrono::steady_clock::now() - std::chrono::seconds(30);

  // if in the last 30 seconds something was sent, don't trigger the keep-alive
  if (!willSend)
    return;
  // if already active, do not interfere
  if (sendActive_)
    return;

  // mark active, so no interference can start
  sendActive_ = true;

  // set empty message
  messageOutHeader_ = MessageHeader::MakeForControlMessage().encode();
  assert(messageOutBody_ == nullptr);

  // send async
  binary_->asyncWrite(&messageOutHeader_, sizeof(messageOutHeader_), [self = SharedFrom(*this)](const networking::SizedTransfer::Result& result) {
    self->handleMessageSent(result);
    });
}

Connection::Connection(std::shared_ptr<Node> node, std::shared_ptr<networking::Connection> binary, boost::asio::io_context& ioContext, RequestHandler* requestHandler)
  : messageInBody_(MaxSizeOfMessage, '\0'), keepAliveTimer_(ioContext), scheduler_(Scheduler::Create(ioContext)), requestor_(Requestor::Create(ioContext, *scheduler_)),
  node_(node), binary_(std::move(binary)), ioContext_(ioContext), requestHandler_(requestHandler) {
  assert(binary_->status() == networking::Transport::ConnectivityStatus::Connected);
  assert(node != nullptr);

  description_ = node->describe() + " connected to " + binary_->remoteAddress();

  if (node->reconnectParameters().has_value()) {
    versionCheckBackoff_.emplace(ioContext_, *node->reconnectParameters());
  }

  this->setStatus(Status::Initializing);

  schedulerAvailableSubscription_ = scheduler_->onAvailable.subscribe([this]() {this->ensureSend(); });
  schedulerExceptionSubscription_ = scheduler_->onError.subscribe([this](const MessageId& id, std::exception_ptr e) { this->handleSchedulerError(id, e); });
}

void Connection::handleMessageReceived(const networking::SizedTransfer::Result& result) {
  if (!result) {
    handleError(result.exception());
    return;
  }

  try {
    auto header = MessageHeader::Decode(messageInHeader_);
    assert(*result == header.length());
    const auto& messageId = header.properties().messageId();

    // Ensure that we keep receiving messages, even if an exception occurs
    PEP_DEFER(this->start());

    // make distinction between message types
    switch (messageId.type().value()) {
    case MessageType::Control:
      // No processing needed: just wait for the next message
      return;

    case MessageType::Response:
      this->processReceivedResponse(messageId.streamId(), header.properties().flags(), this->getReceivedMessageContent(header));
      return;
    case MessageType::Request:
      this->processReceivedRequest(messageId.streamId(), header.properties().flags(), this->getReceivedMessageContent(header));
      return;
    }
  }
  catch (...) {
    PEP_LOG(LogTag, Severity::Warning) << "Failed to process message: " << GetExceptionMessage(std::current_exception());
    // Processed by generic "bad message" handling outside the "catch" clause
  }

  this->handleError(std::make_exception_ptr(boost::system::system_error(boost::system::errc::make_error_code(boost::system::errc::errc_t::bad_message))));
}

std::string Connection::getReceivedMessageContent(const MessageHeader& header) {
  const auto& messageId = header.properties().messageId();

  auto result = messageInBody_.substr(0U, header.length());

  PEP_LOG(LogTag, Severity::Verbose) << "Incoming " << messageId.type().describe() << " ("
    << (result.size() >= sizeof(MessageMagic) ? DescribeMessageMagic(result) : "no valid message magic")
    << ", stream id " << messageId.streamId() << ", " << this->describe() << ")";

  return result;
}

void Connection::close() {
  if (versionCheckBackoff_) {
    versionCheckBackoff_->stop();
  }
  binaryStatusSubscription_.cancel();
  binary_.reset();
  this->clearState(false);
  this->setStatus(Status::Finalizing);
}

void Connection::processReceivedResponse(const StreamId& streamId, const Flags& flags, std::string content) {
  requestor_->processResponse(this->describe(), streamId, flags, std::move(content));
}

void Connection::processReceivedRequest(const StreamId& streamId, const Flags& flags, std::string content) {
  std::shared_ptr<std::string> abValue
    = std::make_shared<std::string>(std::move(content));

  auto it = incomingRequestTails_.find(streamId);
  if (it != incomingRequestTails_.end()) {
    // This is a follow-up chunk for a request whose head we received earlier: have the existing IncomingRequestTail object handle it
    it->second.handleChunk(flags, abValue);
  }
  else if (scheduler_->hasPendingResponseFor(streamId)) {
    // See https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2627
    std::string detail;
    if (abValue->size() >= sizeof(MessageMagic)) {
      detail = DescribeMessageMagic(*abValue);
    }
    else {
      detail = std::to_string(abValue->size()) + "-byte";
    }
    PEP_LOG(LogTag, Severity::Info) << "Dropping (followup?) " << detail << " message for request stream " << streamId.value() << ", which we're already replying to";
  }
  else {
    MessageSequence tail;
    if (flags.has(Flags::Bits::Close)) {
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
      [[maybe_unused]] auto emplaced = incomingRequestTails_.emplace(std::make_pair(streamId, IncomingRequestTail())).second; // Create an IncomingRequestTail object to (cache and) forward those follow-up chunks...
      assert(emplaced);
      tail = CreateObservable<std::shared_ptr<std::string>>([streamId, self = SharedFrom(*this)](rxcpp::subscriber<std::shared_ptr<std::string>> s) { // ... as soon as a subscriber wants them
        auto it = self->incomingRequestTails_.find(streamId);
        if (it != self->incomingRequestTails_.end()) {
          it->second.forwardTo(s);
        }
        else {
          // This code assumes that the tail observable will not be subscribed to after the observable returned by
          // handleXXXRequest has completed or resulted in an error. If you use 'tail' as part of the RX pipeline that will be returned
          // by handleXXXRequest, there should not be a problem.
          PEP_LOG(LogTag, Severity::Warning) << "Subscribed to the 'tail' observable when the incoming request has already been cleaned up";
          assert(false);
        }
        });
    }

    // Have request handled and enqueue return value as response messages
    this->dispatchRequest(streamId, abValue, tail);
  }
}

void Connection::IncomingRequestTail::handleChunk(const Flags& flags, std::shared_ptr<std::string> chunk) {
  if (flags.has(Flags::Bits::Payload)) {
    if (subscriber_) {
      subscriber_->on_next(std::move(chunk));
    } else {
      queuedItems_.emplace_back(std::move(chunk));
    }
  }
  if (flags.has(Flags::Bits::Error)) {
    if (subscriber_) {
      //TODO Why nullptr? Deserialize Error or pass some runtime_error instead?
      PEP_LOG(LogTag, Severity::Warning) << "Received error chunk in request tail";
      subscriber_->on_error(nullptr);
    } else {
      error_ = true;
    }
  } else if (flags.has(Flags::Bits::Close)) {
    if (subscriber_) {
      subscriber_->on_completed();
    } else {
      completed_ = true;
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
    else if (requestHandler_ == nullptr) {
      this->handleError(std::make_exception_ptr(std::runtime_error("No request handler present")));
    }
    else if (!versionValidated_) {
      if (std::any_of(prematureRequests_.begin(), prematureRequests_.end(), [&streamId](const PrematureRequest& candidate) {
        return candidate.streamId == streamId;
        })) {
        throw std::runtime_error("Received multiple premature requests with stream ID " + std::to_string(streamId.value()));
      }
      prematureRequests_.emplace_back(PrematureRequest({ streamId, magic, request, chunks }));
    }
    else {
      responses = requestHandler_->handleRequest(magic, request, chunks);
    }
  } catch (...) {
    responses = rxcpp::observable<>::error<MessageSequence>(std::current_exception());
  }

  if (responses.has_value()) {
    try {
      this->scheduleResponses(streamId, *responses);
    }
    catch (...) {
      PEP_LOG(LogTag, Severity::Error) << "Error scheduling response(s) for received " << DescribeMessageMagic(magic) << " request: " << GetExceptionMessage(std::current_exception())
        << '\n' << "    Connection status is " << ToUnderlying(this->status()) << "; scheduler.available is " << std::boolalpha << scheduler_->available();
      throw;
    }
  }
}

void Connection::scheduleResponses(const StreamId& streamId, MessageBatches responses) {
  scheduler_->push(streamId, responses
    .observe_on(ObserveOnAsio(ioContext_))
    .op(RxBeforeTermination([self = SharedFrom(*this), streamId](std::optional<std::exception_ptr>) {
      self->incomingRequestTails_.erase(streamId);
      })));
}

rxcpp::observable<std::string> Connection::sendRequest(std::shared_ptr<std::string> message, std::optional<MessageBatches> tail) {
  return this->sendRequest(message, tail, false);
}

rxcpp::observable<std::string> Connection::sendRequest(std::shared_ptr<std::string> message, std::optional<MessageBatches> tail, bool isVersionCheck) {
  assert(message);
  // This is a redundant check, such that the caller will receive an exception
  // with a better stack trace.
  if (message->size() >= MaxSizeOfMessage) {
    std::ostringstream msg;
    msg << "Message (" << DescribeMessageMagic(*message)
      << ") to " << describe() << " is too large ("
      << message->length() << ")";
    throw std::runtime_error(msg.str());
  }

  PEP_LOG(LogTag, Severity::Verbose)
    << "Connection::sendRequest: sending "
    << DescribeMessageMagic(*message)
    << " of size " << message->length() << " to " << describe();
  assert(message->size() != 0U);

  return requestor_->send(message, tail, isVersionCheck || versionValidated_, !isVersionCheck);
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
    PEP_LOG(LogTag, Severity::Warning)
      << "Error encountered by " << this->describe()
      << ": " << GetExceptionMessage(exception);
  }

  if (binary_ != nullptr) {
    binary_->close();
  }
}

void Connection::clearState(bool reconnecting) {
  // Let request handlers know that they won't receive further tail segments
  for (auto& incoming : incomingRequestTails_) {
    incoming.second.abort();
  }

  // Cancel sending of previously scheduled request and response messages
  scheduler_->clear();
  // Stop sending keepalive messages
  keepAliveTimer_.cancel();
  keepAliveTimerRunning_ = false;
  // Clear state for outgoing messages
  sendActive_ = false;
  messageOutBody_.reset();
  // Clear state for incoming messages
  versionValidated_ = false;

  // Discard cached incoming requests
  prematureRequests_.clear();
  // Discard pending requests that cannot be re-sent
  requestor_->purge(!reconnecting);
}

void Connection::handleBinaryConnectionEstablished() {
  if (!versionCheckScheduled_) {
    this->performVersionCheck();
  }
}

void Connection::postponeVersionCheck() {
  assert(!versionCheckScheduled_);
  if (versionCheckBackoff_) {
    versionCheckBackoff_->retry([weak = WeakFrom(*this)](boost::system::error_code ec) {
      if (auto self = weak.lock()) {
        self->versionCheckScheduled_ = false;
        if (ec != boost::asio::error::operation_aborted) {
          self->performVersionCheck();
        }
      }
      });
    versionCheckScheduled_ = true;
  }
}

void Connection::performVersionCheck() {
  assert(!versionCheckScheduled_);
  assert(!versionValidated_);

  // Keep instance alive until version check has been performed
  auto self = SharedFrom(*this);

  this->sendRequest(MakeSharedCopy(Serialization::ToString(VersionRequest())), std::nullopt, true)
    .map([](std::string_view response) {return Serialization::FromString<VersionResponse>(response); })
    .observe_on(ObserveOnAsio(ioContext_))
    .subscribe(
      [self](VersionResponse response) {
        self->handleVersionResponse(response);
      },
      [self](std::exception_ptr ep) {
        PEP_LOG(LogTag, Severity::Warning) << "Version check failed: " << GetExceptionMessage(ep);
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
      },
      [self]() {
        if (!self->versionValidated_) {
          auto error = std::make_exception_ptr(ConnectionFailureException::ForVersionCheckFailure("No version response received"));
          self->postponeVersionCheck();
          self->handleError(error);
        }
        else if (self->versionCheckBackoff_) {
          self->versionCheckBackoff_->success();
        }
      });

  // Start accepting messages now to allow connected party to retrieve our version
  start();
}

void Connection::handleVersionResponse(const VersionResponse& response) {
  assert(!versionValidated_);

  auto node = node_.lock();
  if (node == nullptr) {
    throw ConnectionFailureException(boost::system::errc::owner_dead, "Node was discarded before connection could perform version verification");
  }
  if (this->status() != Status::Initializing) {
    throw ConnectionFailureException(boost::system::errc::connection_aborted, "Connection was closed before it could perform version verification");
  }

  assert(binary_ != nullptr);
  try {
    node->vetConnectionWith(this->describe(), binary_->remoteAddress(), response.binary, response.config); // Raises an exception if connection should be refused
  }
  catch (...) {
    this->postponeVersionCheck();
    throw;
  }

  versionValidated_ = true;
  this->setStatus(Status::Initialized);

  // Schedule (re)sendable requests
  requestor_->resend();

  // Handle requests that were received before version check completed (if any)
  auto premature = std::move(prematureRequests_);
  assert(prematureRequests_.empty()); // Move constructor should clear the moved-from vector: see https://stackoverflow.com/a/17735913
  for (auto& request : premature) {
    assert(requestHandler_ != nullptr); // Otherwise premature requests wouldn't have been stored
    try {
      this->scheduleResponses(request.streamId, requestHandler_->handleRequest(request.magic, request.head, request.tail));
    }
    catch (...) {
      PEP_LOG(LogTag, Severity::Error) << "Error scheduling response(s) for premature " << DescribeMessageMagic(request.magic) << " request: " << GetExceptionMessage(std::current_exception());
      throw;
    }
  }
}

void Connection::handleBinaryConnectivityChange(const networking::Connection::ConnectivityChange& change) {
  switch (change.updated) {
  case networking::Transport::ConnectivityStatus::Unconnected: // Prevent compiler warnings due to switch statement not handling all enum values
    assert(false);
    return;
  case networking::Transport::ConnectivityStatus::Reconnecting:
    this->clearState(true);
    this->setStatus(Status::Reinitializing);
    return;
  case networking::Transport::ConnectivityStatus::Connecting:
    assert(!versionValidated_);
    this->setStatus(Status::Initializing);
    return;
  case networking::Transport::ConnectivityStatus::Connected:
    this->handleBinaryConnectionEstablished();
    return;
  case networking::Transport::ConnectivityStatus::Disconnecting:
  case networking::Transport::ConnectivityStatus::Disconnected:
    this->close();
    return;
  }

  throw std::runtime_error("Unsupported binary connection status " + std::to_string(ToUnderlying(change.updated)));
}

std::string Connection::describe() const {
  return description_;
}

bool Connection::isConnected() const noexcept {
  return binary_ != nullptr && binary_->isConnected();
}

std::shared_ptr<Connection> Connection::Open(std::shared_ptr<Node> node, std::shared_ptr<networking::Connection> binary, boost::asio::io_context& ioContext, RequestHandler* requestHandler) {
  assert(binary->isConnected());
  auto result = std::shared_ptr<Connection>(new Connection(node, binary, ioContext, requestHandler));
  assert(result->status() == Status::Initializing);

  // Subscribe the Connection to connectivity changes in the networking::Connection: the constructor couldn't do so because it can't get a shared_ptr to itself
  result->binaryStatusSubscription_ = result->binary_->onConnectivityChange.subscribe([weak = std::weak_ptr<Connection>(result)](const networking::Connection::ConnectivityChange& change) {
    auto self = weak.lock();
    if (self != nullptr) {
      self->handleBinaryConnectivityChange(change);
    }
    });

  // Start initializing: perform version check
  result->handleBinaryConnectionEstablished();

  return result;
}

void Connection::IncomingRequestTail::forwardTo(rxcpp::subscriber<std::shared_ptr<std::string>> subscriber) {
  assert(subscriber_ == nullptr);

  subscriber_ = MakeSharedCopy(subscriber);
  // send items already queued
  for (auto& e : queuedItems_) {
    subscriber_->on_next(std::move(e));
  }
  queuedItems_.clear();
  if (error_) {
    //TODO Why nullptr? Pass deserialized error or some runtime_error instead?
    PEP_LOG(LogTag, Severity::Warning) << "Forwarding error chunk from request tail";
    subscriber_->on_error(nullptr);
  } else if (completed_) {
    subscriber_->on_completed();
  }
}

void Connection::IncomingRequestTail::abort() {
  this->handleChunk(Flags{Flags::Bits::Error | Flags::Bits::Close}, pep::MakeSharedCopy(std::string()));
}

}
