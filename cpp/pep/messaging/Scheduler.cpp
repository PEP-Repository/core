#include <pep/async/OnAsio.hpp>
#include <pep/messaging/MessageHeader.hpp>
#include <pep/messaging/Scheduler.hpp>
#include <pep/serialization/ErrorSerializer.hpp>
#include <pep/serialization/Serialization.hpp>

static const std::string LOG_TAG = "Messaging scheduler";

namespace pep::messaging {

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
  MessageId result(MessageType::REQUEST, streamId);
  this->verifyNewMessageId(result);

  if (!tail) {
    // We'll only send a single message: close stream immediately
    this->emplaceOutgoing(result, Flags::MakePayload(true), request);
  } else {
    // There's more to this request: we'll send tail entries later
    mGenerators[result]; // make sure (entry for) this generator exists so that assertion is satisfied if the (non-final) head message is pop()ped before we can activate the generator
    this->emplaceOutgoing(result, Flags::MakePayload(false), request);
    this->activateGenerator(result, *tail);
  }

  return result;
}

MessageId Scheduler::push(const StreamId& streamId, MessageBatches responses) {
  MessageId result(MessageType::RESPONSE, streamId);
  this->verifyNewMessageId(result);
  this->activateGenerator(result, responses);
  return result;
}

Scheduler::OutgoingMessage Scheduler::pop() {
  assert(!mOutgoing.empty());

  auto result = std::move(mOutgoing.front());
  mOutgoing.pop_front();

  const auto& messageId = result.properties.messageId();

#if BUILD_HAS_DEBUG_FLAVOR()
  if (result.properties.flags().close()) {
    assert(!this->isScheduledMessageId(messageId));
  } else {
    auto closeLater = std::ranges::any_of(mOutgoing, [&messageId](const OutgoingMessage& candidate) {
      return candidate.properties.messageId() == messageId && candidate.properties.flags().close();
      });
    // check if the stream is closed in a later packet in the queue
    // or that there is an observable
    assert(closeLater || mGenerators.find(messageId) != mGenerators.end());
  }
#endif

  // possibly queue next from this stream
  queueNextBatch(messageId);

  return result;
}

bool Scheduler::available() const noexcept {
  return !mOutgoing.empty();
}

bool Scheduler::hasPendingResponseFor(const StreamId& streamId) const {
  return this->isScheduledMessageId(MessageId(MessageType::RESPONSE, streamId));
}

void Scheduler::clear() {
  mGenerators.clear(); // Prevent generators from producing any more stuff before we...
  mOutgoing.clear(); // ...discard stuff that they already produced
}

Scheduler::Scheduler(boost::asio::io_context& io_context) noexcept
  : mIoContext(io_context) {
}

void Scheduler::emplaceOutgoing(const MessageId& messageId, const Flags& flags, std::shared_ptr<std::string> message) {
  if (message->size() >= MAX_SIZE_OF_MESSAGE) {
    throw std::runtime_error("Message too large to enqueue: " + std::to_string(message->size()) + " bytes");
  }
  auto wasEmpty = mOutgoing.empty();
  mOutgoing.emplace_back(MessageProperties(messageId, flags), message);
  if (wasEmpty) {
    onAvailable.notify();
  }
}

void Scheduler::activateGenerator(const MessageId& messageId, MessageBatches batches) {
  assert(mGenerators[messageId].batches.empty()); // Creates the entry if it didn't exist yet, which is no problem since we're going to need it anyway

  auto self = SharedFrom(*this);

  mGenerators[messageId].subscription = batches
    .observe_on(observe_on_asio(mIoContext))
    .subscribe(
      // On_next
      [messageId, self](MessageSequence batch) {
        self->mGenerators[messageId].batches.emplace_back(batch);
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
  if (std::any_of(mOutgoing.begin(), mOutgoing.end(), [&messageId](const OutgoingMessage& entry) { return entry.properties.messageId() == messageId; }))
    return;
  auto it = mGenerators.find(messageId);
  // if not found, do nothing
  if (it == mGenerators.end())
    return;
  auto& batches = it->second.batches;
  // if nothing to send or first observable is already active, do nothing
  if (batches.empty() || batches[0].active)
    return;

  auto self = SharedFrom(*this);
  auto& batch = batches[0];
  batch.active = true; // mark active
  batch.messages
    .observe_on(observe_on_asio(mIoContext))
    .subscribe(
      // on_next
      [messageId, self](std::shared_ptr<std::string> message) {
        self->emplaceOutgoing(messageId, Flags::MakePayload(), message);
      },

      // on_error
      [messageId, self](std::exception_ptr e) {
        std::shared_ptr<std::string> serialized;

        if (e == nullptr) {
          e = std::make_exception_ptr(std::runtime_error("null exception ptr")); // So we don't notify our onError event with a nullptr
        }
        else if (messageId.type() == MessageType::RESPONSE) { // Send Error details back to requestor
          try {
            std::rethrow_exception(e);
          } catch (const Error& err) {
            serialized = MakeSharedCopy(Serialization::ToString(err));
            if (serialized->size() >= MAX_SIZE_OF_MESSAGE) {
              serialized = MakeSharedCopy(Serialization::ToString(Error{"<Error message too large>"}));
            }
          } catch (...) {
            // Ignore: The exception didn't inherit from Error: don't send back details (keep serialized = nullptr)
          }
        }
        else {
          LOG(LOG_TAG, debug) << "Sending error flag to server";
        }

        self->onError.notify(messageId, std::move(e));

        if (serialized == nullptr) {
          std::string message = messageId.type() == MessageType::RESPONSE
                                ? "Internal server error"
                                : "Internal error";
          serialized = MakeSharedCopy(Serialization::ToString(Error{std::move(message)}));
        }
        self->mGenerators.erase(messageId);
        self->emplaceOutgoing(messageId, Flags::MakeError(), serialized);
      },

      // on_complete
      [messageId, sendClose = batch.final, self]() {
        if (sendClose) {
          // We're done sending batches for this message(Id)
          self->mGenerators.erase(messageId);

          bool adjustedInlinePayload = false;
          // try to reuse a packet already in the mOutgoing queue
          for (auto it = self->mOutgoing.rbegin(); it != self->mOutgoing.rend(); ++it) {
            if (it->properties.messageId() == messageId) {
              it->properties = MessageProperties(it->properties.messageId(), it->properties.flags() | Flags::MakeClose());
              adjustedInlinePayload = true;
              break;
            }
          }
          if (!adjustedInlinePayload) {
            self->emplaceOutgoing(messageId, Flags::MakeClose(), std::make_shared<std::string>(""));
          }
        } else {
          // erase current observable (because it is not active anymore)
          auto it = self->mGenerators.find(messageId);
          if (it != self->mGenerators.end()) { // Presumably not an assertion because mGenerators may have been cleared
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
  auto& queue = mGenerators[messageId].batches;
  assert(std::none_of(queue.cbegin(), queue.cend(), [](const Batch& existing) { return existing.final; }));

  // only change inline if we are not processing the stream already
  if (last || queue.empty() || queue.back().active) {
    queue.emplace_back(Batch(last.value_or(rxcpp::observable<>::empty<std::shared_ptr<std::string>>())));
  }
  queue.back().final = true;
  this->queueNextBatch(messageId);
}

bool Scheduler::isScheduledMessageId(const MessageId& messageId) const {
  return std::any_of(mOutgoing.cbegin(), mOutgoing.cend(), [&messageId](const OutgoingMessage& candidate) { return candidate.properties.messageId() == messageId; })
    || mGenerators.contains(messageId);
}

void Scheduler::verifyNewMessageId(const MessageId& messageId) const {
  if (this->isScheduledMessageId(messageId)) {
    throw Error("Can't schedule additional " + messageId.type().describe() + " for stream " + std::to_string(messageId.streamId().value()));
  }
}

}
