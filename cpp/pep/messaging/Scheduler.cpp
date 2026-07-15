#include <pep/async/OnAsio.hpp>
#include <pep/messaging/MessageHeader.hpp>
#include <pep/messaging/Scheduler.hpp>
#include <pep/serialization/ErrorSerializer.hpp>
#include <pep/serialization/Serialization.hpp>

namespace pep::messaging {

namespace {

const std::string LogTag = "Messaging scheduler";

}

Scheduler::Batch::Batch(MessageSequence messages)
  : messages(messages) {
}

Scheduler::OutgoingMessage::OutgoingMessage(MessageProperties properties, std::shared_ptr<std::string> content)
  : properties(properties), content(content) {
}

Scheduler::Generator::~Generator() noexcept {
  if (subscription.is_subscribed()) {
    // Don't let this generator produce any more batches
    subscription.unsubscribe();
  }
}

MessageId Scheduler::push(const StreamId& streamId, std::shared_ptr<std::string> request, std::optional<MessageBatches> tail) {
  MessageId result(MessageType::Request, streamId);
  this->verifyNewMessageId(result);

  if (!tail) {
    // We'll only send a single message: close stream immediately
    this->emplaceOutgoing(result, Flags::ClosingPayload, request);
  } else {
    // There's more to this request: we'll send tail entries later
    generators_[result]; // make sure (entry for) this generator exists so that assertion is satisfied if the (non-final) head message is pop()ped before we can activate the generator
    this->emplaceOutgoing(result, Flags::Payload, request);
    this->activateGenerator(result, *tail);
  }

  return result;
}

MessageId Scheduler::push(const StreamId& streamId, MessageBatches responses) {
  MessageId result(MessageType::Response, streamId);
  this->verifyNewMessageId(result);
  this->activateGenerator(result, responses);
  return result;
}

Scheduler::OutgoingMessage Scheduler::pop() {
  assert(!outgoing_.empty());

  auto result = std::move(outgoing_.front());
  outgoing_.pop_front();

  const auto& messageId = result.properties.messageId();

#if PEP_BUILD_HAS_DEBUG_FLAVOR()
  if (result.properties.flags().has(Flags::Close)) {
    assert(!this->isScheduledMessageId(messageId));
  } else {
    auto closeLater = std::ranges::any_of(outgoing_, [&messageId](const OutgoingMessage& candidate) {
      return candidate.properties.messageId() == messageId && candidate.properties.flags().has(Flags::Close);
      });
    // check if the stream is closed in a later packet in the queue
    // or that there is an observable
    assert(closeLater || generators_.find(messageId) != generators_.end());
  }
#endif

  // possibly queue next from this stream
  queueNextBatch(messageId);

  return result;
}

bool Scheduler::available() const noexcept {
  return !outgoing_.empty();
}

bool Scheduler::hasPendingResponseFor(const StreamId& streamId) const {
  return this->isScheduledMessageId(MessageId(MessageType::Response, streamId));
}

void Scheduler::clear() {
  generators_.clear(); // Prevent generators from producing any more stuff before we...
  outgoing_.clear(); // ...discard stuff that they already produced
}

Scheduler::Scheduler(boost::asio::io_context& io_context) noexcept
  : ioContext_(io_context) {
}

void Scheduler::emplaceOutgoing(const MessageId& messageId, const Flags& flags, std::shared_ptr<std::string> message) {
  if (message->size() >= MaxSizeOfMessage) {
    throw std::runtime_error("Message too large to enqueue: " + std::to_string(message->size()) + " bytes");
  }
  auto wasEmpty = outgoing_.empty();
  outgoing_.emplace_back(MessageProperties(messageId, flags), message);
  if (wasEmpty) {
    onAvailable.notify();
  }
}

void Scheduler::activateGenerator(const MessageId& messageId, MessageBatches batches) {
  assert(generators_[messageId].batches.empty()); // Creates the entry if it didn't exist yet, which is no problem since we're going to need it anyway

  auto self = SharedFrom(*this);

  generators_[messageId].subscription = batches
    .observe_on(ObserveOnAsio(ioContext_))
    .subscribe(
      // On_next
      [messageId, self](MessageSequence batch) {
        self->generators_[messageId].batches.emplace_back(batch);
        self->queueNextBatch(messageId);
      },

      // On_error
      [messageId, self](std::exception_ptr e) {
        MessageSequence batch = rxcpp::observable<>::error<std::shared_ptr<std::string>>(e);
        self->finalizeBatches(messageId, batch);
      },

      // On_complete
      [messageId, self]() {
        self->finalizeBatches(messageId);
      });
}

void Scheduler::queueNextBatch(const MessageId& messageId) {
  // if there are messages queued for this message id, wait with requesting the next batch
  if (std::any_of(outgoing_.begin(), outgoing_.end(), [&messageId](const OutgoingMessage& entry) { return entry.properties.messageId() == messageId; }))
    return;
  auto it = generators_.find(messageId);
  // if not found, do nothing
  if (it == generators_.end())
    return;
  auto& batches = it->second.batches;
  // if nothing to send or first observable is already active, do nothing
  if (batches.empty() || batches[0].active)
    return;

  auto self = SharedFrom(*this);
  auto& batch = batches[0];
  batch.active = true; // mark active
  batch.messages
    .observe_on(ObserveOnAsio(ioContext_))
    .subscribe(
      // on_next
      [messageId, self](std::shared_ptr<std::string> message) {
        self->emplaceOutgoing(messageId, Flags::Payload, message);
      },

      // on_error
      [messageId, self](std::exception_ptr e) {
        std::shared_ptr<std::string> serialized;

        if (e == nullptr) {
          e = std::make_exception_ptr(std::runtime_error("null exception ptr")); // So we don't notify our onError event with a nullptr
        }
        else if (messageId.type() == MessageType::Response) { // Send Error details back to requestor
          try {
            std::rethrow_exception(e);
          } catch (const Error& err) {
            serialized = MakeSharedCopy(Serialization::ToString(err));
            if (serialized->size() >= MaxSizeOfMessage) {
              serialized = MakeSharedCopy(Serialization::ToString(Error{"<Error message too large>"}));
            }
          } catch (...) {
            // Ignore: The exception didn't inherit from Error: don't send back details (keep serialized = nullptr)
          }
        }
        else {
          PEP_LOG(LogTag, Severity::Debug) << "Sending error flag to server";
        }

        self->onError.notify(messageId, std::move(e)); //NOLINT(performance-move-const-arg) libc++ doesn't support moving exception_ptr

        if (serialized == nullptr) {
          std::string message = messageId.type() == MessageType::Response
                                ? "Internal server error"
                                : "Internal error";
          serialized = MakeSharedCopy(Serialization::ToString(Error{std::move(message)}));
        }
        self->generators_.erase(messageId);
        self->emplaceOutgoing(messageId, Flags::Error, serialized);
      },

      // on_complete
      [messageId, sendClose = batch.final, self]() {
        if (sendClose) {
          // We're done sending batches for this message(Id)
          self->generators_.erase(messageId);

          bool adjustedInlinePayload = false;
          // try to reuse a packet already in the outgoing_ queue
          for (auto it = self->outgoing_.rbegin(); it != self->outgoing_.rend(); ++it) {
            if (it->properties.messageId() == messageId) {
              it->properties = MessageProperties(it->properties.messageId(),it->properties.flags().withClose());
              adjustedInlinePayload = true;
              break;
            }
          }
          if (!adjustedInlinePayload) {
            self->emplaceOutgoing(messageId, Flags::Close, std::make_shared<std::string>(""));
          }
        } else {
          // erase current observable (because it is not active anymore)
          auto it = self->generators_.find(messageId);
          if (it != self->generators_.end()) { // Presumably not an assertion because generators_ may have been cleared
            auto& batches = it->second.batches;
            assert(!batches.empty());
            if (!batches.empty() && batches[0].active)
              batches.erase(batches.begin());
          }
          // if nothing is queued, we can request the next batch
          self->queueNextBatch(messageId);
        }
      });
}

void Scheduler::finalizeBatches(const MessageId& messageId, const std::optional<MessageSequence>& last) {
  auto& queue = generators_[messageId].batches;
  assert(std::none_of(queue.cbegin(), queue.cend(), [](const Batch& existing) { return existing.final; }));

  // only change inline if we are not processing the stream already
  if (last || queue.empty() || queue.back().active) {
    queue.emplace_back(Batch(last.value_or(rxcpp::observable<>::empty<std::shared_ptr<std::string>>())));
  }
  queue.back().final = true;
  this->queueNextBatch(messageId);
}

bool Scheduler::isScheduledMessageId(const MessageId& messageId) const {
  return std::any_of(outgoing_.cbegin(), outgoing_.cend(), [&messageId](const OutgoingMessage& candidate) { return candidate.properties.messageId() == messageId; })
    || generators_.contains(messageId);
}

void Scheduler::verifyNewMessageId(const MessageId& messageId) const {
  if (this->isScheduledMessageId(messageId)) {
    throw Error("Can't schedule additional " + messageId.type().describe() + " for stream " + std::to_string(messageId.streamId().value()));
  }
}

}
