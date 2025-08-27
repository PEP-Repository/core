#pragma once

#include <pep/messaging/MessageProperties.hpp>
#include <pep/messaging/MessageSequence.hpp>
#include <pep/utils/Event.hpp>
#include <pep/utils/Shared.hpp>
#include <pep/async/IoContext_fwd.hpp>

namespace pep::messaging {

/**
 * @brief Schedules outgoing request and response messages into a single queue(-like interface).
 * @remark Most importantly, this class ensures that outgoing MessageBatches are processed at the right time and
 *         that errors are propagated properly.
 * @remark Message senders (i.e. "Connection" instances) will want to "pop" a message from the Scheduler when they're
 *         ready to send the next message. They'll want to check the "available" method before to "pop"ping a new
 *         message, and do so
 *         - when they're done sending a previous message, and
 *         - when a new message has been scheduled, i.e. when the "onAvailable" event is notified.
 */
class Scheduler : public std::enable_shared_from_this<Scheduler>, public SharedConstructor<Scheduler> {
  friend class SharedConstructor<Scheduler>;

public:
  /**
   * @brief A scheduled message that's ready to be sent.
   */
  struct OutgoingMessage {
    MessageProperties properties;
    std::shared_ptr<std::string> content;

    OutgoingMessage(MessageProperties properties, std::shared_ptr<std::string> content);
  };

  /**
   * @brief Schedules a request message and associated tail entries for sending.
   * @param id the stream ID to associate with the request. Must be unique.
   * @param request the main message representing the request
   * @param tail optional followup messages associated with the request
   * @return the message ID assigned to the request
   */
  MessageId push(const StreamId& id, std::shared_ptr<std::string> request, std::optional<MessageBatches> tail = std::nullopt);

  /**
   * @brief Schedules response message(s) for sending.
   * @param id the stream ID associated with (the request to which this is) the response
   * @param responses the messages to send
   * @return the message ID assigned to the response
   */
  MessageId push(const StreamId& id, MessageBatches responses);

  /**
   * @brief Retrieves the next message to be sent, removing it from the queue(-like interface)
   * @return the message to be sent next
   * @remark only call when the "available" method returns TRUE.
   */
  OutgoingMessage pop();

  /**
   * @brief Determines if there's at least one message ready to be sent.
   * @return TRUE if there's a message ready to be sent; FALSE if not
   * @remark indicates whether the "pop" method may be invoked
   */
  bool available() const noexcept;

  /**
   * @brief Determines if there's a response pending for a specific stream (ID).
   * @param streamId the ID of the stream
   * @return TRUE if there's (a) response message(s) and/or batch(es) pending; FALSE if not
   */
  bool hasPendingResponseFor(const StreamId& streamId) const;

  /**
   * @brief Discards all messages from the scheduler, bringing it back to its initial (pristine) state.
   */
  void clear();

  /**
   * @brief Occurs when a message becomes available (for sending) and none were available before.
   */
  const Event<Scheduler> onAvailable;

  /**
   * @brief Occurs when an error message is scheduled to be sent.
   * @remark The error message (is put at the back of the queue, so) may not be the next one that will be "pop"ped.
   * @remark The exception_ptr represents the actual error, but the outgoing message (which will be produced by the
   *         "pop" method) will lack details if the exception is not an instance of (a class that inherits from) the
   *         network-portable "Error" class.
   */
  const Event<Scheduler, MessageId, std::exception_ptr> onError;

private:
  explicit Scheduler(boost::asio::io_context& io_context) noexcept;

  void emplaceOutgoing(const MessageId& messageId, const Flags& flags, std::shared_ptr<std::string> payload);

  void activateGenerator(const MessageId& messageId, MessageBatches batches);
  void queueNextBatch(const MessageId& messageId);
  void finalizeBatches(const MessageId& messageId, const std::optional<MessageSequence>& last = std::nullopt);

  bool isScheduledMessageId(const MessageId& messageId) const;
  void verifyNewMessageId(const MessageId& messageId) const;

  struct Batch {
    bool final = false;
    bool active = false;
    MessageSequence messages;

    explicit Batch(MessageSequence messages);
  };

  struct Generator : boost::noncopyable {
    rxcpp::composite_subscription subscription;
    std::vector<Batch> batches;

    ~Generator() noexcept;
  };

  boost::asio::io_context& mIoContext;
  std::map<MessageId, Generator> mGenerators;
  std::deque<OutgoingMessage> mOutgoing; // We need to (be able to) iterate over elements, so we can't use a simple std::queue<>.
};

}
