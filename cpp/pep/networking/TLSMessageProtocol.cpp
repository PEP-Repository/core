#include <pep/utils/Defer.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Log.hpp>
#include <pep/async/OnAsio.hpp>
#include <pep/networking/NetworkingSerializers.hpp>
#include <pep/networking/TLSMessageProtocol.hpp>
#include <pep/async/CreateObservable.hpp>
#include <pep/utils/MiscUtil.hpp>
#include <pep/async/RxEnsureProgress.hpp>
#include <pep/utils/Shared.hpp>
#include <pep/serialization/ErrorSerializer.hpp>

#include <boost/bind/bind.hpp>
#include <rxcpp/rx-lite.hpp>

namespace pep {

namespace {

const std::string LOG_TAG("MessageProtocol");

constexpr const size_t TLS_MESSAGE_MAX_SIZE =
// #1156: use larger message size in release builds so things will fail in debug builds (on dev boxes) before they bring prod down
#if BUILD_HAS_RELEASE_FLAVOR
  2 *
#endif
  // TODO: reduce (back) to 1 Mb (i.e. remove multiplier by 2).
  // Value was increased as a temporary fix for (production) problems: see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2238#note_30480
  2 * 1024 * 1024 - 4;

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

const size_t TLSMessageProtocol::MAX_SIZE_OF_MESSAGE = TLS_MESSAGE_MAX_SIZE;

struct TLSMessageProtocol::Connection::MessageIn {
  MessageHeader header;
  char body[TLS_MESSAGE_MAX_SIZE];
};

TLSMessageProtocol::Connection::FollowupBatch::FollowupBatch(MessageSequence messages)
  : messages(messages) {
}

TLSMessageProtocol::Connection::FollowupBatch TLSMessageProtocol::Connection::FollowupBatch::MakeFinal(MessageSequence messages) {
  auto result = FollowupBatch(messages);
  result.final = true;
  return result;
}

TLSMessageProtocol::Connection::OutgoingMessage::OutgoingMessage(MessageProperties properties, std::shared_ptr<std::string> content)
  : properties(properties), content(content) {
}

TLSMessageProtocol::Connection::OutstandingRequest::OutstandingRequest(std::shared_ptr<std::string> message,
  std::optional<rxcpp::observable<MessageSequence>> generator,
  std::optional<rxcpp::subscription> subscription,
  rxcpp::subscriber<std::string> subscriber)
  : message(message), generator(generator), subscription(subscription), subscriber(subscriber) {
}


size_t TLSMessageProtocol::Connection::boostBytesToReceive(const boost::system::error_code& error, size_t bytes) {
  LOG(LOG_TAG, severity_level::verbose)
    << "TLSMessageProtocol::Connection::boostBytesToReceive: "
    << "receiving message from " << describe()
    << "; received " << bytes << " bytes so far.";

  bytes += mMessageInStart;

  if (error) {
    LOG(LOG_TAG, severity_level::verbose)
      << " \\__ error! " << error << ", that is, " << error.message();
    return 0; // passes error to boostOnMessageReceived.
  }

  // first 8 bytes are the header of the message
  if (bytes < sizeof(MessageHeader))
    return sizeof(MessageHeader); // TODO: subtract number of bytes already received?

  auto messageLength = ntohl(mMessageIn->header.messageLength);
  auto todo = messageLength + sizeof(MessageHeader);

  LOG(LOG_TAG, severity_level::verbose)
    << "TLSMessageProtocol::Connection::boostBytesToReceive: message from "
    << describe() << " will have size "
    << messageLength << " of which we have received "
    << bytes - sizeof(MessageHeader) << " bytes already.";

  if (bytes >= todo)
    return 0;

  return todo - mMessageInStart;
}

void TLSMessageProtocol::Connection::ensureSend() {
  LOG(LOG_TAG, severity_level::verbose) << "TLSMessageProtocol::Connection::ensureSend (mState=" << mState << ",sendActive=" << mSendActive << ",mOutgoing.size=" << mOutgoing.size() << ",mOutstandingRequests.size=" << mOutstandingRequests.size() << ",receivedRequests.size=" << mReceivedRequests.size() << ",to=" << describe() << ")";
  if (mState < TLSProtocol::Connection::HANDSHAKE_DONE || mState > TLSProtocol::Connection::CONNECTED)
    return;
  if (mSendActive)
    return;
  if (mOutgoing.size() == 0)
    return;
  mSendActive = true;

  // we have to send in two stages, first a header of 8 bytes consiting of a message length and a message id
  auto entry = std::move(mOutgoing.front());
  MessageProperties properties = entry.properties;
  mMessageOutBody = entry.content;
  assert(mMessageOutBody);
  mOutgoing.pop_front();

#if BUILD_HAS_DEBUG_FLAVOR
  assert(!((properties & FLAG_PAYLOAD) && (properties & FLAG_ERROR))); // not both payload and error
  assert(!(properties & FLAG_ERROR) || (properties & FLAG_CLOSE)); // error implies close (this can be changed, but with porting Rx over the network makes no sense now)
  if (properties & FLAG_CLOSE) {
    for (auto it = mOutgoing.begin(); it != mOutgoing.end(); ++it) {
      assert(GetMessageId(it->properties) != GetMessageId(properties));
    }
    assert(mFollowupBatches.find(GetMessageId(properties)) == mFollowupBatches.end());
  } else {
    bool closeLater = false;
    for (auto it = mOutgoing.begin(); it != mOutgoing.end(); ++it) {
      closeLater |= GetMessageId(it->properties) == GetMessageId(properties) && (it->properties & FLAG_CLOSE) == FLAG_CLOSE;
    }
    // check if the stream is closed in a later packet in the queue
    // or that there is an observable
    assert(closeLater || mFollowupBatches.find(GetMessageId(properties)) != mFollowupBatches.end());
  }
#endif

  LOG(LOG_TAG, severity_level::verbose) << "TLSMessageProtocol::Connection::ensureSend outgoing message streamId=" << GetStreamId(properties) << " (to " << describe() << ")";

  if (mMessageOutBody->size() >= MAX_SIZE_OF_MESSAGE) {
    std::ostringstream msg;
    msg << "Message queued to sent from chunk-generator is too large.  ("
        << "Size=" << mMessageOutBody->size() << ", Type="
        << DescribeMessageMagic(*mMessageOutBody)
        << ")";
    throw std::runtime_error(msg.str());
  }

  mMessageOutHeader.messageLength = htonl(static_cast<uint32_t>(mMessageOutBody->length()));
  mMessageOutHeader.properties = htonl(properties);

  // after the message header is written, call the second stage to send the body
  boost::asio::async_write(*socket, boost::asio::buffer(&mMessageOutHeader, sizeof(mMessageOutHeader)),
                           boost::bind(&TLSMessageProtocol::Connection::boostOnHeaderSent, SharedFrom(*this),
                                       boost::asio::placeholders::error,
                                       boost::asio::placeholders::bytes_transferred));

  // possibly queue next from this stream
  queueNextBatch(GetMessageId(properties));
}

void TLSMessageProtocol::Connection::queueNextBatch(MessageId messageId) {
  assert(GetMessageId(messageId) == messageId); // Ensure we don't get bytes outside the message ID

  // if there are messages queued for this stream id, wait with requesting the next batch
  if (std::any_of(mOutgoing.begin(), mOutgoing.end(), [messageId](const OutgoingMessage& entry) { return GetMessageId(entry.properties) == messageId;}))
    return;
  auto it = mFollowupBatches.find(messageId);
  // if not found, do nothing
  if (it == mFollowupBatches.end())
    return;
  auto& batches = it->second;
  // if nothing to send or first observable is already active, do nothing
  if (batches.empty() || batches[0].active)
    return;

  auto& batch = batches[0];
  batch.active = true; // mark active
  batch.messages
    .observe_on(observe_on_asio(*getProtocol()->getIOservice()))
    .subscribe(
      // on_next
      [messageId, self = SharedFrom(*this)](std::shared_ptr<std::string> reply) {
    MessageProperties messageProperties = messageId | FLAG_PAYLOAD;
    if (reply->size() >= MAX_SIZE_OF_MESSAGE) {
      throw std::runtime_error("Message too large to enqueue: " + std::to_string(reply->size()) + " bytes");
    }
    self->mOutgoing.emplace_back(messageProperties, reply);
    self->ensureSend();
  },

      // on_error
    [messageId, self = SharedFrom(*this)](std::exception_ptr e) {
    MessageProperties messageProperties = messageId | FLAG_ERROR | FLAG_CLOSE;
    try {
      if (e)
        std::rethrow_exception(e);
      throw std::runtime_error("null exception ptr");
    }
    catch (const RequestRefusedException& err) {
      assert(GetMessageType(messageProperties) == TYPE_RESPONSE);
      self->queueErrorAsNext(messageProperties, err, "Refused");
    }
    catch (const Error& err) {
      assert(GetMessageType(messageProperties) == TYPE_RESPONSE); // Request sender shouldn't raise a network-portable Error instance
      self->queueErrorAsNext(messageProperties, err, "Error");
    }
    catch (...) { // TODO: don't assume that we're sending a response: the batch might just as well be a generator associated with a request
      LOG(LOG_TAG, severity_level::error)
        << "Unexpected exception (handling: " << self->describe() << "):"
        << GetExceptionMessage(e);
      self->getProtocol()->mUncaughtReadExceptions++;
      self->mOutgoing.emplace_back(messageProperties, std::make_shared<std::string>(""));
    }
    self->mFollowupBatches.erase(messageId);
    self->ensureSend();
  },

    // on_complete
    [messageId, sendClose = batch.final, self = SharedFrom(*this)]() {
    if (sendClose) {
      bool adjustedInlinePayload = false;
      // try to reuse a packet already in the mOutgoing queue
      for (auto it = self->mOutgoing.rbegin(); it != self->mOutgoing.rend(); ++it) {
        if (GetMessageId(it->properties) == messageId) {
          it->properties |= FLAG_CLOSE;
          adjustedInlinePayload = true;
          break;
        }
      }
      if (!adjustedInlinePayload) {
        MessageProperties responseProperties = messageId | FLAG_CLOSE;
        self->mOutgoing.emplace_back(responseProperties, std::make_shared<std::string>(""));
      }
      self->mFollowupBatches.erase(messageId);
      self->ensureSend();
    }
    else {
      // erase current observable (because it is not active anymore)
      auto it = self->mFollowupBatches.find(messageId);
      if (it != self->mFollowupBatches.end()) { // Presumably not an assertion because onConnectFailed may have cleared mFollowupBatches
        auto& batches = it->second;
        assert(!batches.empty());
        if (!batches.empty() && batches[0].active)
          batches.erase(batches.begin());
      }
      // if nothing is queued, we can request the next batch
      self->queueNextBatch(messageId);
    }
  });
}

void TLSMessageProtocol::Connection::queueErrorAsNext(MessageProperties responseProperties, const Error& err, const std::string& logCaption) {
  assert(GetMessageType(responseProperties) == TYPE_RESPONSE);
  assert(responseProperties & FLAG_ERROR);
  assert(responseProperties & FLAG_CLOSE);

  LOG(LOG_TAG, severity_level::warning)
    << logCaption << " (handling " << describe() << "): "
    << err.what();
  auto str = std::make_shared<std::string>(Serialization::ToString(err));
  // FIXME: probably should be a special exception that the exception was too big
  if (str->size() >= MAX_SIZE_OF_MESSAGE)
    str->clear();
  mOutgoing.emplace_back(responseProperties, str);
}

void TLSMessageProtocol::Connection::start() {
  // boost::asio::async_read is going to memcpy into our MessageIn struct
  static_assert(sizeof(MessageIn) == sizeof(MessageIn::header) + sizeof(MessageIn::body), "Incoming message struct must be packed, i.e. may not contain padding");

  boost::asio::async_read(*socket,
                          boost::asio::buffer(mMessageIn.get() + mMessageInStart, sizeof(*mMessageIn) - mMessageInStart),
                          boost::bind(&TLSMessageProtocol::Connection::boostBytesToReceive,
                            SharedFrom(*this),
                                      boost::asio::placeholders::error,
                                      boost::asio::placeholders::bytes_transferred),
                          boost::bind(&TLSMessageProtocol::Connection::boostOnMessageReceived,
                            SharedFrom(*this),
                                      boost::asio::placeholders::error,
                                      boost::asio::placeholders::bytes_transferred));

  // start keep-alive
  if (!mKeepAliveTimerRunning) {
    mKeepAliveTimerRunning = true;
    mKeepAliveTimer.expires_from_now(boost::posix_time::seconds(30));
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

  mLastSend = std::chrono::steady_clock::now();

  ensureSend();
}

void TLSMessageProtocol::Connection::boostOnKeepAliveTimerExpired(const boost::system::error_code& error) {
  // timer cancelled
  if (error) {
    //std::cerr << "boostOnKeepAliveTimerExpired: " << describe() << " -> timer CANCELLED: " << error.value() << "/" << error.message() << std::endl;
    return;
  }

  mKeepAliveTimer.expires_from_now(boost::posix_time::seconds(30));
  mKeepAliveTimer.async_wait(boost::bind(&TLSMessageProtocol::Connection::boostOnKeepAliveTimerExpired, SharedFrom(*this), boost::asio::placeholders::error));

  bool willSend = mLastSend <= std::chrono::steady_clock::now() - std::chrono::seconds(30);

  //std::cerr << "boostOnKeepAliveTimerExpired: " << describe() << " -> " << willSend << " (" << std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - lastSend).count() << " seconds ago)" << std::endl;

  // if in the last 30 seconds something was send, don't trigger the keep-alive
  if (!willSend)
    return;
  // if already active, do not intefere
  if (mSendActive)
    return;

  // mark active, so no interference can start
  mSendActive = true;

  // set empty message
  mMessageOutHeader.messageLength = htonl(0);
  mMessageOutHeader.properties = htonl(CONTROL_STREAM_ID);
  assert(mMessageOutBody == nullptr);

  // send async
  boost::asio::async_write(*socket, boost::asio::buffer(&mMessageOutHeader, sizeof(mMessageOutHeader)),
    boost::bind(&TLSMessageProtocol::Connection::boostOnMessageSent,
      SharedFrom(*this),
      boost::asio::placeholders::error,
      boost::asio::placeholders::bytes_transferred));
}

TLSMessageProtocol::Connection::Connection(std::shared_ptr<TLSMessageProtocol> protocol)
  : TLSProtocol::Connection(protocol), mMessageIn(std::make_unique<MessageIn>()), mKeepAliveTimer(*protocol->getIOservice()) {
}

TLSMessageProtocol::Connection::~Connection() {
  if (mOutstandingRequests.size() != 0) {
    LOG(LOG_TAG, severity_level::error)
      << "outstanding requests list is not empty:";
    for (const auto& kv : mOutstandingRequests) {
      std::string msgType = "(too short)";
      auto msg = kv.second.message;
      if (msg->size() >= sizeof(MessageMagic))
        msgType = DescribeMessageMagic(*msg);
      LOG(LOG_TAG, severity_level::error)
        << " streamid " << kv.first
        << " " << msgType;
    }
    assert(false);
  }
}

void TLSMessageProtocol::Connection::boostOnMessageReceived(const boost::system::error_code& error, size_t bytes) {
  if (error) {
    onConnectFailed(error);
    return;
  }

  // read size of message and stream id
  MessageLength messageLength = ntohl(mMessageIn->header.messageLength);
  if (messageLength > sizeof(mMessageIn->body)) {
    LOG(LOG_TAG, severity_level::warning)
      << "Received " << DescribeMessageMagic(mMessageIn->body) << " message that's too large: " << messageLength;
    onConnectFailed(error); // TODO should we use this error path?
    return;
  }
  MessageProperties properties = ntohl(mMessageIn->header.properties);
  auto streamId = GetStreamId(properties);

  /* Each request gets a streamId, which is an unsigned int, or uint32_t.
     If the streamId == CONTROL_STREAM_ID, it is a "control message", for now a keep-alive.
     Otherwise:
       If the properties have a 0 as leftmost bit, the message is a request.
       If the properties have has a 1 as leftmost bit, the message is a response. When a server sends a response in answer to a request,
       it takes the streamId of the request and toggles the leftmost bit to signify that this message is a response. */

  // highest bit 0/1 (request/response), 0/1 (will be next packet / end of stream), 0/1 (no error / error), 0/1 (no payload / has payload)

  // make distinction if the message is a request or response
  if (streamId == CONTROL_STREAM_ID) {
    // control messages, keep-alive
    assert(bytes == sizeof(MessageHeader));
  } else {
    MessageFlags flags = GetMessageFlags(properties);
    if (GetMessageType(properties) == TYPE_RESPONSE) {
      this->handleReceivedResponse(mMessageIn->body, messageLength, streamId, flags);
    }
    else {
      this->handleReceivedRequest(mMessageIn->body, messageLength, streamId, flags);
    }
  }

  // we have to compensate for the bytes already in the buffer
  bytes += mMessageInStart;
  mMessageInStart = bytes - (sizeof(MessageHeader) + messageLength);
  memmove(mMessageIn.get(), mMessageIn.get() + sizeof(MessageHeader) + messageLength, mMessageInStart);
  start();
}

void TLSMessageProtocol::Connection::logIncomingMessage(const std::string& type, StreamId streamId, const std::string& content) {
  assert(content.empty() || content.size() >= sizeof(MessageMagic));

  LOG(LOG_TAG, severity_level::verbose) << "Incoming " << type << " ("
    << (content.empty() ? std::string("without message magic") : DescribeMessageMagic(content))
    << ", stream id " << streamId << ", " << this->describe() << ")";
}


void TLSMessageProtocol::Connection::handleReceivedResponse(const char* content, MessageLength length, StreamId streamId, MessageFlags flags) {
  assert(GetMessageFlags(flags) == flags); // Ensure the bitset contains nothing other than flags
  bool close = flags & FLAG_CLOSE;
  bool error = flags & FLAG_ERROR;
  bool payload = flags & FLAG_PAYLOAD;

  auto it = mOutstandingRequests.find(streamId);
  if (it != mOutstandingRequests.end()) {
    // got response, send it back using the rx subscriber
    auto body = std::string(content, length);
    this->logIncomingMessage("response", streamId, body);
    auto subscriber = it->second.subscriber;
    if (error || close)
      mOutstandingRequests.erase(it);

    // NOTE don't use "it" from this point onwards.
    if (error) {
      auto err = Error::ReconstructIfDeserializable(body);
      if (err == nullptr) {
        // Backward compatible: if no Error instance could be deserialized, report on an empty instance
        err = std::make_exception_ptr(Error(std::string()));
      }
      LOG(LOG_TAG, severity_level::error)
        << "Received an error! (stream id " << streamId
        << " to " << describe() << "): "
        << GetExceptionMessage(err);
      subscriber.on_error(err);
    }
    else {
      if (payload)
        subscriber.on_next(std::move(body));
      if (close)
        subscriber.on_completed();
    }

    if (error || close) {
      LOG(LOG_TAG, severity_level::verbose)
        << "Closed stream " << streamId
        << " (to " << describe() << ")";
    }
  }
  else {
    // log wrong payload for non existing stream
    LOG(LOG_TAG, warning) << "received response for non existent stream: " << streamId << " (to " << describe() << ")";
  }
}

void TLSMessageProtocol::Connection::handleReceivedRequest(const char* content, MessageLength length, StreamId streamId, MessageFlags flags) {
  assert(GetMessageFlags(flags) == flags); // Ensure the bitset contains nothing other than flags

  std::shared_ptr<std::string> abValue
    = std::make_shared<std::string>(content, length);
  this->logIncomingMessage("request", streamId, *abValue);

  auto it = mReceivedRequests.find(streamId);
  if (it != mReceivedRequests.end()) {
    // This is a follow-up chunk for a request whose head we received earlier: have the existing ReceivedRequest object handle it
    it->second.handleChunk(flags, abValue);
  }
  else {
    MessageSequence chunks;
    if (GetMessageFlags(flags) & FLAG_CLOSE) {
      // An empty request with a close flag for an unknown stream can safely
      // be ignored, and is probably a superfluous close message, see #1188.
      if (abValue->size() == 0) {
        return;
      }
      // This is a (non-empty) request without followup chunks
      chunks = rxcpp::observable<>::empty<std::shared_ptr<std::string>>();
    }
    else {
      // This is (the head of) a request that has follow-up chunks
      [[maybe_unused]] auto emplaced = mReceivedRequests.emplace(std::make_pair(streamId, ReceivedRequest())).second; // Create a ReceivedRequest object to (cache and) forward those follow-up chunks...
      assert(emplaced);
      chunks = CreateObservable<std::shared_ptr<std::string>>([streamId, self = SharedFrom(*this)](rxcpp::subscriber<std::shared_ptr<std::string>> s) { // ... as soon as a subscriber wants them
        auto it = self->mReceivedRequests.find(streamId);
        if (it != self->mReceivedRequests.end()) {
          it->second.forwardTo(s);
        }
        else {
          // This code assumes that the chunks-observable (also called 'generator') will not be subscribed to after the observable returned by
          // handleXXXRequest has completed or resulted in an error. If you use 'generator' as part of the RX pipeline that will be returned
          // by handleXXXRequest, there should not be a problem.
          LOG(LOG_TAG, warning) << "Subscribed to the 'chunks' observable when the incoming request has already been cleaned up";
          assert(false);
        }
      });
    }

    // Have request handled and enqueue return value as response messages
    MessageId responseId = streamId | TYPE_RESPONSE;
    this->dispatchToHandler(abValue, chunks)
      .observe_on(observe_on_asio(*getProtocol()->getIOservice()))
      .subscribe(
        // On_next
        [responseId, self = SharedFrom(*this)](MessageSequence replies) {
      self->mFollowupBatches[responseId].emplace_back(replies);
      self->queueNextBatch(responseId);
    },

        // On_error
      [streamId, responseId, self = SharedFrom(*this)](std::exception_ptr e) {
      self->mReceivedRequests.erase(streamId);
      self->enqueueErrorReply(responseId, e);
    },

      // On_complete
      [streamId, responseId, self = SharedFrom(*this)]() {
      self->mReceivedRequests.erase(streamId);
      auto& queue = self->mFollowupBatches[responseId];
      // only change inline if we are not processing the stream already
      if (queue.empty() || queue.back().active) {
        queue.emplace_back(FollowupBatch::MakeFinal());
      }
      else {
        queue.back().final = true;
      }
      self->queueNextBatch(responseId);
    });
  }
}

void TLSMessageProtocol::Connection::ReceivedRequest::handleChunk(MessageFlags flags, std::shared_ptr<std::string> chunk) {
  assert(GetMessageFlags(flags) == flags); // Ensure the bitset contains nothing other than flags
  bool close = flags & FLAG_CLOSE;
  bool error = flags & FLAG_ERROR;
  bool payload = flags & FLAG_PAYLOAD;

  if (payload) {
    if (mSubscriber) {
      mSubscriber->on_next(std::move(chunk));
    }
    else {
      mQueuedItems.emplace_back(std::move(chunk));
    }
  }
  if (error) {
    if (mSubscriber) {
      mSubscriber->on_error(nullptr);
    }
    else {
      mError = true;
    }
  }
  else if (close) {
    if (mSubscriber) {
      mSubscriber->on_completed();
    }
    else {
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

void TLSMessageProtocol::Connection::enqueueErrorReply(MessageId messageId, std::exception_ptr e) {
  // TODO: assert((messageId & TYPE_RESPONSE) == TYPE_RESPONSE)

  MessageSequence replyGenerator;
  try {
    if (e)
      std::rethrow_exception(e);
    throw std::runtime_error("null exception ptr");
  }
  catch (const Error& err) {
    replyGenerator = rxcpp::observable<>::error<std::shared_ptr<std::string>>(err);
  }
  catch (...) {
    LOG(LOG_TAG, severity_level::error)
      << "Stripping error details from reply (handling: " << describe() << "):"
      << GetExceptionMessage(e);
    replyGenerator = rxcpp::observable<>::error<std::shared_ptr<std::string>>(std::runtime_error("unhandled exception"));
  }
  mFollowupBatches[messageId].emplace_back(FollowupBatch::MakeFinal(replyGenerator));
  queueNextBatch(messageId);
}


rxcpp::observable<TLSMessageProtocol::MessageSequence> TLSMessageProtocol::handleVersionRequest(std::shared_ptr<VersionRequest> request [[maybe_unused]]) {
  VersionResponse response{ BinaryVersion::current, ConfigVersion::Current() };
  MessageSequence retval = rxcpp::observable<>::from(std::make_shared<std::string>(Serialization::ToString(response)));
  return rxcpp::observable<>::from(retval);
}

rxcpp::observable<TLSMessageProtocol::MessageSequence> TLSMessageProtocol::handlePingRequest(std::shared_ptr<PingRequest> request) {
  PingResponse resp(request->mId);
  MessageSequence retval = rxcpp::observable<>::from(std::make_shared<std::string>(Serialization::ToString(resp)));
  return rxcpp::observable<>::from(retval);
}

TLSMessageProtocol::TLSMessageProtocol(std::shared_ptr<Parameters> parameters)
  : TLSProtocol(parameters) {
  this->registerHousekeepingRequestHandler(&TLSMessageProtocol::handlePingRequest);
  this->registerHousekeepingRequestHandler(&TLSMessageProtocol::handleVersionRequest);
}

rxcpp::observable<TLSMessageProtocol::MessageSequence> TLSMessageProtocol::handleRequest(MessageMagic magic, std::shared_ptr<std::string> message, MessageSequence chunks) {
  auto position = mRequestHandlingMethods.find(magic);
  if (position == mRequestHandlingMethods.cend()) {
    throw Error("Unsupported message type " + DescribeMessageMagic(magic));
  }
  return position->second->handle(*this, message, chunks);
}

WaitGroup::Action TLSMessageProtocol::Connection::pendVersionVerification() {
  mVersionCorrect = false;
  mVersionVerification = WaitGroup::Create();
  return mVersionVerification->add("version verification");
}

rxcpp::observable<TLSMessageProtocol::MessageSequence> TLSMessageProtocol::Connection::dispatchToHandler(
    std::shared_ptr<std::string> request, MessageSequence chunks) {
  try {
    auto magic = PopMessageMagic(*request);

    // Housekeeping requests are always handled
    if (this->getProtocol()->mHousekeepingRequests.find(magic) != this->getProtocol()->mHousekeepingRequests.cend()) {
      return this->getProtocol()->handleRequest(magic, request, chunks);
    }

    // Other request handlers are invoked after we've made sure that we're dealing with a party that has the correct version
    return mVersionVerification->delayObservable<MessageSequence>([weak = pep::static_pointer_cast<Connection>(weak_from_this()), magic, request, chunks]()->rxcpp::observable<MessageSequence> {
      try {
        auto self = weak.lock();
        if (!self) {
          throw std::runtime_error("Connection closed before request could be handled");
        }
        if (!self->mVersionCorrect) {
          throw RequestRefusedException("Refusing to handle request from connected party with incompatible network protocol version");
        }
        return self->getProtocol()->handleRequest(magic, request, chunks);
      }
      catch (...) {
        return rxcpp::observable<>::error<MessageSequence>(std::current_exception());
      }
    });
  } catch (...) {
    return rxcpp::observable<>::error<MessageSequence>(std::current_exception());
  }
}

rxcpp::composite_subscription TLSMessageProtocol::Connection::subscribeToOutgoingRequestGenerator(rxcpp::observable<MessageSequence> generator, MessageId messageId) {
  assert(GetMessageId(messageId) == messageId);
  assert(GetMessageType(messageId) == TYPE_REQUEST);

  return generator
    .observe_on(observe_on_asio(*getProtocol()->getIOservice()))
    .subscribe(
      // on_next
      [messageId, self = SharedFrom(*this)](MessageSequence strings) {
    self->mFollowupBatches[messageId].emplace_back(strings);
    self->queueNextBatch(messageId);
  },

      // on_error
      [messageId, self = SharedFrom(*this)](std::exception_ptr e) {
    self->enqueueErrorReply(messageId, e); // TODO: we're dealing with our own (outgoing) request here. What is connected party going to do with an error reply?!?
  },
    
    // on_complete
    [messageId, self = SharedFrom(*this)] {
    auto& queue = self->mFollowupBatches[messageId];
    // only change inline if we are not processing the stream already
    if (queue.empty() || queue.back().active) {
      queue.emplace_back(FollowupBatch::MakeFinal());
    } else {
      queue.back().final = true;
    }
    self->queueNextBatch(messageId);
  });
}

TLSMessageProtocol::Connection::StreamId TLSMessageProtocol::Connection::getNewRequestStreamId() {
  // use the previous request ID to start looking for a new one
  auto result = mPreviousRequestStreamId;

  do {
    // ensure that ID differs from the previously generated one
    ++result;
    // ensure that our increment didn't spill into the (high) bits reserved for stuff other than the stream ID
    result = GetStreamId(result);
    // ensure that we didn't wrap to zero
    if (result == CONTROL_STREAM_ID) {
      result = CONTROL_STREAM_ID + 1U;
    }
  } while (mOutstandingRequests.find(result) != mOutstandingRequests.end()); // ensure that we don't recycle IDs of requests that we haven't received a reply to
  
  // ensure that a future call doesn't produce this ID again
  return mPreviousRequestStreamId = result;
}

rxcpp::observable<std::string> TLSMessageProtocol::Connection::sendRequest(std::shared_ptr<std::string> message, std::optional<rxcpp::observable<MessageSequence>> generator) {
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

  return CreateObservable<std::string>([self = SharedFrom(*this), message, generator](rxcpp::subscriber<std::string> s) {
    MessageId messageId = self->getNewRequestStreamId();
    assert(GetStreamId(messageId) == messageId);
    assert(GetMessageType(messageId) == TYPE_REQUEST);

    std::optional<rxcpp::subscription> subscription;
    if (!generator) {
      // We'll only send a single message: close stream immediately
      self->mOutgoing.emplace_back(messageId | FLAG_PAYLOAD | FLAG_CLOSE, message);
    } else {
      // There's more to this request: we'll send generator ("tail") entries later
      self->mOutgoing.emplace_back(messageId | FLAG_PAYLOAD, message);
      self->mFollowupBatches[messageId]; // make sure entry for batches exists (assumption are checked in ensureSend() that if a payload is send, a stream could send a close later)
      subscription = self->subscribeToOutgoingRequestGenerator(*generator, messageId);
    }

    // remember request so a reply can be sent to the correct handler, and to retransmit request in case of reconnection
    self->mOutstandingRequests.emplace(GetStreamId(messageId), OutstandingRequest(message, generator, subscription, s));

    // trigger send
    self->ensureSend();
  }).subscribe_on(observe_on_asio(*getProtocol()->getIOservice()));
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
  mOutgoing.clear(); // resend all messages of all requests that have not been completely handled
  mSendActive = false;
  mFollowupBatches.clear();
  mMessageInStart = 0;
  mMessageOutBody.reset();
  mKeepAliveTimerRunning = false;
  mKeepAliveTimer.cancel();
  TLSProtocol::Connection::onConnectFailed(error);
}

bool TLSMessageProtocol::Connection::resendOutstandingRequests() {
  if (mState == TLSProtocol::Connection::SHUTDOWN) {
    return false;
  }

  if (!mOutgoing.empty()) {
    throw std::runtime_error("Pending requests can only be re-sent when there are no outgoing messages"); // I.e. invoke only after onConnectFailed, which clears mOutgoing
  }

  // resend single message requests, but remove the requests with 
  // a chunk generator, since currently we cannot re-generate 
  // the messages already send, see #1225
  for (auto it = this->mOutstandingRequests.begin(); it != this->mOutstandingRequests.end(); /* sic */) {
    // confused? See the example of:
    //  https://en.cppreference.com/w/cpp/container/map/erase

    auto& streamid = it->first;
    auto& request = it->second;

    if (request.generator) {
      // remove this request from the list, but before we do this,
      // unsubscribe from the generator
      if (request.subscription && request.subscription->is_subscribed())
        request.subscription->unsubscribe();

      // notify caller of the failure, but not directly!, since this might
      // affect the this->requests map while we're looping over it
      //
      // it might be more consistent to put an "observe_on(observe_on_asio(..))"
      // on the observable returned by sendRequest, but I do not oversee the
      // all the consequences that might have
      this->getProtocol()->getIOservice()->post(
        [subscriber = std::move(request.subscriber)]{
          subscriber.on_error(std::make_exception_ptr(
                Error("Aborting multi-message request due to onConnectFailed")));
        });

      it = this->mOutstandingRequests.erase(it); // Position "it" on the next item, i.e. the one after the one that we erased
    }
    else {
      // without a generator there should be no subscription to it
      assert(!request.subscription);

      assert(request.message->size() != 0U);
      this->mOutgoing.emplace_back(streamid | FLAG_PAYLOAD | FLAG_CLOSE, request.message);

      it++; // Move "it" to the next item
    }
  }

  return true;
}

bool TLSMessageProtocol::IncompatibleRemote::operator<(const IncompatibleRemote& rhs) const {
  return std::tie(this->address, this->binary, this->config) < std::tie(rhs.address, rhs.binary, rhs.config);
}

void TLSMessageProtocol::handleVersionResponse(const boost::asio::ip::address& address, const VersionResponse& response) {
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
    msg += " connection between incompatible remote (" + response.binary.getProtocolChecksum() + " at " + address.to_string()
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
  boost::asio::io_service& ioService = *getProtocol()->getIOservice();
  RxEnsureProgress(ioService,
    "Version verification for " + this->describe() + " connected to " + this->getSocket().remote_endpoint().address().to_string(),
    sendRequest<VersionResponse>(VersionRequest()))
    .observe_on(observe_on_asio(ioService))
    .subscribe(
      [self](VersionResponse response) {
        self->getProtocol()->handleVersionResponse(self->getSocket().remote_endpoint().address(), response);
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
