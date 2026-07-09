#pragma once

#include <pep/async/FakeVoid.hpp>
#include <pep/networking/Connection.hpp>
#include <pep/networking/ExponentialBackoff.hpp>
#include <pep/messaging/HousekeepingMessages.hpp>
#include <pep/messaging/MessageHeader.hpp>
#include <pep/messaging/RequestHandler.hpp>
#include <pep/messaging/Requestor.hpp>
#include <pep/serialization/Serialization.hpp>

#include <boost/asio/steady_timer.hpp>
#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-reduce.hpp>

namespace pep::messaging {

class Node;

class Connection : public pep::LifeCycler, public std::enable_shared_from_this<Connection> {
  friend class Node;

public:
  using ConnectivityStatus = networking::Connection::ConnectivityStatus;
  using ConnectivityChange = networking::Connection::ConnectivityChange;

  using Attempt = networking::ConnectivityAttempt<Connection>;

  std::string describe() const;

  bool isConnected() const noexcept;

  ~Connection() noexcept override { this->close(); }

  // sends a message, the future is the reply as received by the server
  rxcpp::observable<std::string> sendRequest(std::shared_ptr<std::string> message, std::optional<MessageBatches> tail = {});

  const Event<Connection, std::exception_ptr> onUncaughtReadException;

  // ******************** State and callbacks for message exchange using pep::networking classes ********************
private:
  bool sendActive_ = false;

  // helper buffer for output
  EncodedMessageHeader messageOutHeader_{};
  std::shared_ptr<std::string> messageOutBody_;

  // buffer to read incoming messages
  EncodedMessageHeader messageInHeader_{};
  std::string messageInBody_;

  // sending a message is in two stages: first sending a header, afterwards sending a body. These are completion handlers associated with them
  void handleHeaderSent(const networking::SizedTransfer::Result& result);
  void handleMessageSent(const networking::SizedTransfer::Result& result);

  // receiving a message is in two stages: first receiving a header, afterwards receiving a body. These are completion handlers associated with them
  void handleHeaderReceived(const networking::SizedTransfer::Result& result);
  void handleMessageReceived(const networking::SizedTransfer::Result& result);

  std::string getReceivedMessageContent(const MessageHeader& header);

  void start();

  // ******************** State and callback for keep-alive timer ********************
private:
  bool keepAliveTimerRunning_ = false;
  boost::asio::steady_timer keepAliveTimer_;
  std::chrono::steady_clock::time_point lastSend_;

  void handleKeepAliveTimerExpired(const boost::system::error_code& error);

  // ******************** Scheduling and sending of messages (requests and responses) ********************
private:
  void ensureSend();
  void handleSchedulerError(const MessageId& id, std::exception_ptr error);

  std::shared_ptr<Scheduler> scheduler_;
  EventSubscription schedulerAvailableSubscription_;
  EventSubscription schedulerExceptionSubscription_;

  // ******************** Outgoing requests ********************
private:
  std::shared_ptr<Requestor> requestor_;

  rxcpp::observable<std::string> sendRequest(std::shared_ptr<std::string> message, std::optional<MessageBatches> tail, bool isVersionCheck);
  void processReceivedResponse(const StreamId& streamId, const Flags& flags, std::string content);

  // ******************** Incoming requests ********************
private:
  class IncomingRequestTail {
  private:
    std::vector<std::shared_ptr<std::string>> queuedItems_; // items that are queued if there is no subscriber to push them to
    std::shared_ptr<rxcpp::subscriber<std::shared_ptr<std::string>>> subscriber_;
    bool error_ = false;
    bool completed_ = false;

  public:
    void handleChunk(const Flags& flags, std::shared_ptr<std::string> content);
    void forwardTo(rxcpp::subscriber<std::shared_ptr<std::string>> subscriber);
    void abort();
  };
  std::map<StreamId, IncomingRequestTail> incomingRequestTails_;

  struct PrematureRequest {
    StreamId streamId;
    MessageMagic magic;
    std::shared_ptr<std::string> head;
    MessageSequence tail;
  };
  std::vector<PrematureRequest> prematureRequests_;

  void processReceivedRequest(const StreamId& streamId, const Flags& flags, std::string content);
  void dispatchRequest(const StreamId& streamId, std::shared_ptr<std::string> request, MessageSequence chunks);
  void scheduleResponses(const StreamId& streamId, MessageBatches responses);

  // ******************** Version verification ********************
private:
  bool versionValidated_ = false;
  bool versionCheckScheduled_ = false;
  std::optional<ExponentialBackoff> versionCheckBackoff_;

  void handleBinaryConnectionEstablished();
  void postponeVersionCheck();
  void performVersionCheck();
  MessageBatches handleVersionRequest(std::shared_ptr<std::string> request [[maybe_unused]], MessageSequence chunks [[maybe_unused]]);
  void handleVersionResponse(const VersionResponse& response);

  // ******************** Miscellaneous ********************
private:
  bool prepareBodyTransfer(const networking::SizedTransfer::Result& headerResult);
  void handleError(std::exception_ptr exception);
  void handleBinaryConnectivityChange(const networking::Connection::ConnectivityChange& change);
  void clearState(bool reconnecting);

protected:
  void close();

private:
  Connection(std::shared_ptr<Node> node, std::shared_ptr<networking::Connection> binary, boost::asio::io_context& ioContext, RequestHandler* requestHandler);
  static std::shared_ptr<Connection> Open(std::shared_ptr<Node> node, std::shared_ptr<networking::Connection> binary, boost::asio::io_context& ioContext, RequestHandler* requestHandler);

  std::weak_ptr<Node> node_;
  std::string description_;
  std::shared_ptr<networking::Connection> binary_;
  EventSubscription binaryStatusSubscription_;
  boost::asio::io_context& ioContext_;
  RequestHandler* requestHandler_;
};


}
