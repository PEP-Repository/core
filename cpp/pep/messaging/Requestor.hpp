#pragma once

#include <pep/messaging/Scheduler.hpp>

namespace pep::messaging {

/**
* @brief Helper class to associate request observables and response message(chunk)s with request+response cycles.
*        Ensures that response chunks are marshalled into the correct response observable, and that (some)
*        request+response cycles can be retried if network connectivity is (lost and) re-established.
*/
class Requestor : public std::enable_shared_from_this<Requestor>, public SharedConstructor<Requestor>, boost::noncopyable {
  friend class SharedConstructor<Requestor>;

private:
  boost::asio::io_context& mIoContext;
  Scheduler& mScheduler;

  StreamId mPreviousRequestStreamId = StreamId::BeforeFirst();

  StreamId getNewRequestStreamId();

  // request messages that are active in the connection
  // both content of message (for retransmission), as what to do with message is remembered
  struct Entry {
    std::shared_ptr<std::string> message;
    std::optional<MessageBatches> tail;
    bool resendable = false;
    rxcpp::subscriber<std::string> subscriber;

    Entry(std::shared_ptr<std::string> message,
      std::optional<MessageBatches> tail,
      bool resendable,
      rxcpp::subscriber<std::string> subscriber);
  };

  std::map<StreamId, Entry> mEntries;
  void schedule(decltype(mEntries)::value_type& entry);

  Requestor(boost::asio::io_context& io_context, Scheduler& scheduler);

public:
  ~Requestor() noexcept;

  /**
  * @brief Sends a request, returning the response as an observable<> of serialized messages.
  * @param request The (serialized) request to send.
  * @param tail Any followup messages associated with the request. Pass std::nullopt if the request consists of a single (head) message.
  * @param immediately Whether or not the message should be scheduled immediately.
  * @param resend Whether the message is eligible to be re-sent after the requestor is purge()d.
  * @return An observable that emits the (serialized) response messages produced by the request's handler.
  */
  rxcpp::observable<std::string> send(std::shared_ptr<std::string> request, std::optional<MessageBatches> tail, bool immediately, bool resend);

  /**
  * @brief Marshals a response (message) into the observable that emits the associated request's responses.
  * @param recipient (A description of) the receiving connection.
  * @param streamId The stream ID associated with the response message.
  * @param flags The flags received with the response message.
  * @param body The contents of the response message
  */
  void processResponse(const std::string& recipient, const StreamId& streamId, const Flags& flags, std::string body);

  /**
  * @brief Counts the number of requests that have been sent but for which no (full) response has been received yet.
  * @return The number of pending requests.
  */
  size_t pending() const noexcept { return mEntries.size(); }

  /**
  * @brief Discards pending requests that cannot be re-sent, producing an error on the associated observable<>.
  */
  void purge();

  /**
  * @brief Re-schedules pending requests that can be re-sent.
  */
  void resend();
};

}
