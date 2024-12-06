#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/core/demangle.hpp>

#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-reduce.hpp>

#include <pep/networking/TLSProtocol.hpp>
#include <pep/networking/HousekeepingMessages.hpp>
#include <pep/async/WaitGroup.hpp>
#include <pep/serialization/Serialization.hpp>

#include <optional>

namespace pep {

class TLSMessageProtocol : public TLSProtocol {
public:
  using MessageSequence = rxcpp::observable<std::shared_ptr<std::string>>;

  static const size_t MAX_SIZE_OF_MESSAGE;

  class Connection : public TLSProtocol::Connection {
    // ******************** Public interface ********************
  public:
    Connection(std::shared_ptr<TLSMessageProtocol> protocol);
    ~Connection() override;

    // sends a message, the future is the reply as received by the server
    virtual rxcpp::observable<std::string> sendRequest(std::shared_ptr<std::string> message, std::optional<rxcpp::observable<MessageSequence>> chunkGenerator = {});

    // Convenience function to serialize and send a message, wait for a
    // response of a certain type and deserialize it.  Assumes there is a
    // single response to the request.
    template<typename response_type, typename request_type>
    rxcpp::observable<response_type> sendRequest(request_type request);

    // ******************** Message property encoding and decoding ********************
  private:
    // Every message is sent and received with some properties encoded into a single integral value
    using MessageProperties = uint32_t;

    // MessageProperties uses the (single) high bit to indicate message type
    using MessageType = MessageProperties;
    static_assert(sizeof(MessageType) == 4, "needed for the code to be able to make the distinction between requests (highest bit 0) and responses (highest bit 1)");
    static constexpr MessageType TYPE_REQUEST = 0x00000000;
    static constexpr MessageType TYPE_RESPONSE = 0x80000000;
    static constexpr MessageType TYPE = TYPE_REQUEST | TYPE_RESPONSE;

    // MessageProperties uses (the next-highest) three bits for state-related flags
    using MessageFlags = MessageProperties;
    static constexpr MessageFlags FLAG_CLOSE = 0x40000000; // This is the last piece of the (possibly multi-part) message
    static constexpr MessageFlags FLAG_ERROR = 0x20000000; // The sending party encountered an error constructing or sending the (possibly multi-part) message. Implies FLAG_CLOSE.
    static constexpr MessageFlags FLAG_PAYLOAD = 0x10000000; // The message includes content
    static constexpr MessageFlags FLAGS = FLAG_CLOSE | FLAG_ERROR | FLAG_PAYLOAD;

    // Remaining bits in MessageProperties represent a unique (serial) number for every request+response cycle
    using StreamId = MessageProperties;
    // A MessageId is the combination (bitwise OR) of the StreamId and MessageType: allows us to distinguish between our request NNN, and our response to someone else's request NNN
    using MessageId = MessageProperties;

    static inline MessageType GetMessageType(MessageProperties properties) noexcept { return properties & TYPE; }
    static inline MessageFlags GetMessageFlags(MessageProperties properties) noexcept { return properties & FLAGS; }
    static inline MessageId GetMessageId(MessageProperties properties) noexcept { return properties & ~FLAGS; }
    static inline StreamId GetStreamId(MessageProperties properties) noexcept { return GetMessageId(properties) & ~TYPE; }

    // ******************** State and callbacks for message exchange using boost::asio ********************
  private:
    // every packet has a size of this type, in network order
    using MessageLength = uint32_t;

#pragma pack(push, 1)
    struct MessageHeader {
      MessageLength messageLength;
      MessageProperties properties;
    };
#pragma pack(pop)

    bool mSendActive = false;

    // helper buffer for output
    MessageHeader mMessageOutHeader;
    std::shared_ptr<std::string> mMessageOutBody;

    // buffer to read incoming messages
    std::size_t mMessageInStart = 0;
    struct MessageIn;
    std::unique_ptr<MessageIn> mMessageIn;

    // sending a packet is in two stages: first sending a header, afterwards sending a body. These are completion handlers associated with them
    void boostOnHeaderSent(const boost::system::error_code& error, size_t /*bytes_transferred*/);
    void boostOnMessageSent(const boost::system::error_code& error, size_t /*bytes_transferred*/);

    // helper function to determine if an object has been received fully
    size_t boostBytesToReceive(const boost::system::error_code& error, size_t bytes_transferred);
    // if an object has been received, read object and trigger an action
    void boostOnMessageReceived(const boost::system::error_code& error, size_t bytes);

    void start();

    // ******************** State and callback for keep-alive timer ********************
  private:
    bool mKeepAliveTimerRunning = false;
    boost::asio::deadline_timer mKeepAliveTimer;
    std::chrono::steady_clock::time_point mLastSend;

    void boostOnKeepAliveTimerExpired(const boost::system::error_code& error);

    // ******************** Messages (requests and responses) that are waiting to be transmitted ASAP ********************
  private:
    struct OutgoingMessage {
      MessageProperties properties;
      std::shared_ptr<std::string> content;

      OutgoingMessage(MessageProperties properties, std::shared_ptr<std::string> content);
    };
    std::deque<OutgoingMessage> mOutgoing; // We need to (be able to) iterate over elements, so we can't use a simple std::queue<>.

    void ensureSend();
    void queueErrorAsNext(MessageProperties responseProperties, const Error& err, const std::string& logCaption);

    // ******************** Follow-up messages that are waiting to be transmitted after the previous outgoing message has been sent ********************
  private:
    struct FollowupBatch {
      bool final = false;
      bool active = false;
      MessageSequence messages;

      explicit FollowupBatch(MessageSequence messages);
      static FollowupBatch MakeFinal(MessageSequence messages = rxcpp::observable<>::empty<std::shared_ptr<std::string>>());
    };
    std::map<MessageId, std::vector<FollowupBatch>> mFollowupBatches;

    void queueNextBatch(MessageId messageId);

    // ******************** Outgoing requests ********************
  private:
    static constexpr const StreamId CONTROL_STREAM_ID = 0;
    StreamId mPreviousRequestStreamId = CONTROL_STREAM_ID;

    StreamId getNewRequestStreamId();

    // request messages that are active in the connection
    // both content of message (for retransmission), as what to do with message is remembered
    struct OutstandingRequest {
      std::shared_ptr<std::string> message;
      std::optional<rxcpp::observable<MessageSequence>> generator;
      std::optional<rxcpp::subscription> subscription;
      rxcpp::subscriber<std::string> subscriber;

      OutstandingRequest(std::shared_ptr<std::string> message,
        std::optional<rxcpp::observable<MessageSequence>> generator,
        std::optional<rxcpp::subscription> subscription,
        rxcpp::subscriber<std::string> subscriber);
    };
    std::map<StreamId, OutstandingRequest> mOutstandingRequests;

    rxcpp::composite_subscription subscribeToOutgoingRequestGenerator(rxcpp::observable<MessageSequence> generator, MessageId messageId);
    void handleReceivedResponse(const char* content, MessageLength length, StreamId streamId, MessageFlags flags);

  protected:
    bool resendOutstandingRequests();

    // ******************** Incoming requests ********************
  private:
    class ReceivedRequest {
    private:
      std::vector<std::shared_ptr<std::string>> mQueuedItems; // items that are queued if there is no subscriber to push them to
      std::shared_ptr<rxcpp::subscriber<std::shared_ptr<std::string>>> mSubscriber;
      bool mError = false;
      bool mCompleted = false;

    public:
      void handleChunk(MessageFlags flags, std::shared_ptr<std::string> content);
      void forwardTo(rxcpp::subscriber<std::shared_ptr<std::string>> subscriber);
    };
    std::map<StreamId, ReceivedRequest> mReceivedRequests;

    void handleReceivedRequest(const char* content, MessageLength length, StreamId streamId, MessageFlags flags);
    rxcpp::observable<MessageSequence> dispatchToHandler(std::shared_ptr<std::string> request, MessageSequence chunks);
    void enqueueErrorReply(MessageId messageId, std::exception_ptr e);

    // ******************** Version verification ********************
  private:
    std::shared_ptr<WaitGroup> mVersionVerification;
    bool mVersionCorrect = false;

    WaitGroup::Action pendVersionVerification();

  protected:
    void onHandshakeSuccess() override;

    // ******************** Miscellaneous ********************
  protected:
    void onConnectFailed(const boost::system::error_code& error) override;
    std::shared_ptr<TLSMessageProtocol> getProtocol() const noexcept { return std::static_pointer_cast<TLSMessageProtocol>(TLSProtocol::Connection::getProtocol()); }
    void logIncomingMessage(const std::string& type, StreamId streamId, const std::string& content);

  }; // End of nested Connection class

protected:
  TLSMessageProtocol(std::shared_ptr<Parameters> parameters);

  /**
   * @brief Registers one or more member functions as request handlers.
   * @param instance Caller must pass `this` here, so that ThisT resolves to the specific type of the caller.
   * @param methods pointers to the member functions of ThisT that will be registered as handlers.
   * @tparam ThisT The specific type of the TLSMessageProtocol instance on which the methods are registered.
   */
  template <typename ThisT, typename... MethodPtrTs>
  void registerRequestHandlers(ThisT* instance, MethodPtrTs... methods) {
    assert(instance == this);
    static_assert(sizeof...(methods) >= 1);
    (RegisterSingleRequestHandler(*instance, methods), ...);
  }

private:
  class RequestHandlingMethod {
  public:
    virtual rxcpp::observable<MessageSequence> handle(TLSMessageProtocol& instance, std::shared_ptr<std::string> message, MessageSequence chunks) const = 0;
    inline virtual ~RequestHandlingMethod() {}
  };

  template <typename DeclaringT, typename RequestT>
  class UnaryRequestHandlingMethod : public RequestHandlingMethod {
  public:
    using Method = rxcpp::observable<MessageSequence> (DeclaringT::*)(std::shared_ptr<RequestT>);

  private:
    Method mMethod;

  public:
    explicit UnaryRequestHandlingMethod(Method method) : mMethod(method) {}

    rxcpp::observable<MessageSequence> handle(TLSMessageProtocol& instance, std::shared_ptr<std::string> message, MessageSequence chunks) const override {
      DeclaringT& downcast = static_cast<DeclaringT&>(instance); // TODO: ensure this cast is valid
      auto request = std::make_shared<RequestT>(Serialization::FromString<RequestT>(*message, false));
      return (downcast.*mMethod)(request);
    }
  };

  template <typename DeclaringT, typename RequestT>
  class BinaryRequestHandlingMethod : public RequestHandlingMethod {
  public:
    using Method = rxcpp::observable<MessageSequence> (DeclaringT::*)(std::shared_ptr<RequestT>, MessageSequence);

  private:
    Method mMethod;

  public:
    explicit BinaryRequestHandlingMethod(Method method) : mMethod(method) {}

    rxcpp::observable<MessageSequence> handle(TLSMessageProtocol& instance, std::shared_ptr<std::string> message, MessageSequence chunks) const override {
      DeclaringT& downcast = static_cast<DeclaringT&>(instance); // TODO: ensure this cast is valid
      auto request = std::make_shared<RequestT>(Serialization::FromString<RequestT>(*message, false));
      return (downcast.*mMethod)(request, chunks);
    }
  };

  rxcpp::observable<MessageSequence> handleRequest(MessageMagic magic, std::shared_ptr<std::string> message, MessageSequence chunks);
  rxcpp::observable<MessageSequence> handleVersionRequest(std::shared_ptr<VersionRequest> request);
  rxcpp::observable<MessageSequence> handlePingRequest(std::shared_ptr<PingRequest> request);

  void handleVersionResponse(const boost::asio::ip::address& address, const VersionResponse& response);

  template <typename DeclaringT, typename RequestT>
  static void RegisterSingleRequestHandler(
      DeclaringT& instance, rxcpp::observable<MessageSequence> (DeclaringT::*method)(std::shared_ptr<RequestT>)) {
    using Handler = UnaryRequestHandlingMethod<DeclaringT, RequestT>;
    instance.mRequestHandlingMethods.emplace(MessageMagician<RequestT>::GetMagic(), std::make_shared<Handler>(method));
  }

  template <typename DeclaringT, typename RequestT>
  static void RegisterSingleRequestHandler(
      DeclaringT& instance,
      rxcpp::observable<MessageSequence> (DeclaringT::*method)(std::shared_ptr<RequestT>, MessageSequence)) {
    using Handler = BinaryRequestHandlingMethod<DeclaringT, RequestT>;
    instance.mRequestHandlingMethods.emplace(MessageMagician<RequestT>::GetMagic(), std::make_shared<Handler>(method));
  }

  template <typename RequestT>
  void registerHousekeepingRequestHandler(
      rxcpp::observable<MessageSequence> (TLSMessageProtocol::*method)(std::shared_ptr<RequestT> request)) {
    mHousekeepingRequests.emplace(MessageMagician<RequestT>::GetMagic());
    RegisterSingleRequestHandler(*this, method);
  }

protected:
  unsigned int getNumberOfUncaughtReadExceptions() const noexcept;

private:
  struct IncompatibleRemote {
    boost::asio::ip::address address;
    std::string binary;
    std::string config;

    bool operator<(const IncompatibleRemote& rhs) const; // Allow instances of this type to be stored in e.g. an std::set<>
  };

  std::set<IncompatibleRemote> mIncompatibleRemotes;
  unsigned int mUncaughtReadExceptions = 0;
  std::unordered_map<MessageMagic, std::shared_ptr<RequestHandlingMethod>> mRequestHandlingMethods;
  std::unordered_set<MessageMagic> mHousekeepingRequests;
};

template<typename response_type, typename request_type>
rxcpp::observable<response_type>
TLSMessageProtocol::Connection::sendRequest(request_type request) {
  std::shared_ptr<bool> done = std::make_shared<bool>(false);
  return this->sendRequest(std::make_shared<std::string>(Serialization::ToString(std::move(request)))).map(
    [done](std::string msg) {
      if (*done) {
        std::ostringstream message;
        message
          << "Unexpected double reply to "
          << boost::core::demangle(typeid(request_type).name());
        throw std::runtime_error(message.str());
      }
      *done = true;
      if (msg.size() < sizeof(MessageMagic)) {
        std::ostringstream message;
        message
          << "Unexpected short message in response to request "
          << boost::core::demangle(typeid(request_type).name())
          << ": expected "
          << boost::core::demangle(typeid(response_type).name());
        throw std::runtime_error(message.str());
      }
      auto magic = GetMessageMagic(msg);
      if (magic != MessageMagician<response_type>::GetMagic()) {
        std::ostringstream message;
        message
          << "Unexpected response message type to request "
          << boost::core::demangle(typeid(request_type).name())
          << ": expected "
          << boost::core::demangle(typeid(response_type).name())
          << ", but got "
          << DescribeMessageMagic(msg);
        throw std::runtime_error(message.str());
      }
      return Serialization::FromString<response_type>(std::move(msg));
    }).last();
}

}
