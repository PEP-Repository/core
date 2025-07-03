#pragma once

#include <boost/core/demangle.hpp>

#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-reduce.hpp>

#include <pep/networking/TLSProtocol.hpp>
#include <pep/networking/HousekeepingMessages.hpp>
#include <pep/networking/MessageHeader.hpp>
#include <pep/networking/RequestHandler.hpp>
#include <pep/networking/Requestor.hpp>
#include <pep/async/WaitGroup.hpp>
#include <pep/serialization/Serialization.hpp>

#include <optional>

namespace pep {

class TLSMessageProtocol : public TLSProtocol, public RequestHandler {
public:
  class Connection : public TLSProtocol::Connection {
    // ******************** Public interface ********************
  public:
    Connection(std::shared_ptr<TLSMessageProtocol> protocol);

    // sends a message, the future is the reply as received by the server
    virtual rxcpp::observable<std::string> sendRequest(std::shared_ptr<std::string> message, std::optional<MessageBatches> tail = {});

    // Convenience function to serialize and send a message, wait for a
    // response of a certain type and deserialize it.  Assumes there is a
    // single response to the request.
    template<typename response_type, typename request_type>
    rxcpp::observable<response_type> sendRequest(request_type request);

    // ******************** State and callbacks for message exchange using boost::asio ********************
  private:
    bool mSendActive = false;

    // helper buffer for output
    EncodedMessageHeader mMessageOutHeader;
    std::shared_ptr<std::string> mMessageOutBody;

    // buffer to read incoming messages
    EncodedMessageHeader mMessageInHeader;
    std::string mMessageInBody;

    // sending a packet is in two stages: first sending a header, afterwards sending a body. These are completion handlers associated with them
    void boostOnHeaderSent(const boost::system::error_code& error, size_t /*bytes_transferred*/);
    void boostOnMessageSent(const boost::system::error_code& error, size_t /*bytes_transferred*/);

    // receiving a message is in two stages: first receiving a header, afterwards receiving a body. These are completion handlers associated with them
    void boostOnHeaderReceived(const boost::system::error_code& error, size_t bytes);
    void boostOnMessageReceived(const boost::system::error_code& error, size_t bytes);

    void start();

    // ******************** State and callback for keep-alive timer ********************
  private:
    bool mKeepAliveTimerRunning = false;
    boost::asio::steady_timer mKeepAliveTimer;
    decltype(mKeepAliveTimer)::time_point mLastSend;

    void boostOnKeepAliveTimerExpired(const boost::system::error_code& error);

    // ******************** Scheduling and sending of messages (requests and responses) ********************
  private:
    void ensureSend();
    void handleSchedulerError(const MessageId& id, std::exception_ptr error);

    std::shared_ptr<Scheduler> mScheduler;
    EventSubscription mSchedulerAvailableSubscription;
    EventSubscription mSchedulerExceptionSubscription;

    // ******************** Outgoing requests ********************
  private:
    std::shared_ptr<Requestor> mRequestor;
    void handleReceivedResponse(const char* content, const MessageHeader& header);

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

    void handleReceivedRequest(const char* content, const MessageHeader& header);
    MessageBatches dispatchToHandler(std::shared_ptr<std::string> request, MessageSequence tail);

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
    void logIncomingMessage(const std::string& type, const StreamId& streamId, const std::string& content);

  }; // End of nested Connection class

protected:
  TLSMessageProtocol(std::shared_ptr<Parameters> parameters);

private:
  MessageBatches handleVersionRequest(std::shared_ptr<VersionRequest> request [[maybe_unused]]);
  MessageBatches handlePingRequest(std::shared_ptr<PingRequest> request);

  void handleVersionResponse(const boost::asio::ip::address& address, const VersionResponse& response, const std::string& description);

  template <typename RequestT>
  void registerHousekeepingRequestHandler(
      MessageBatches (TLSMessageProtocol::*method)(std::shared_ptr<RequestT> request)) {
    mHousekeepingRequests.emplace(MessageMagician<RequestT>::GetMagic());
    RegisterRequestHandlers(*this, method);
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
