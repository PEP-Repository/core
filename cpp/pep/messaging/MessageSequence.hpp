#pragma once

#include <pep/serialization/Serialization.hpp>
#include <pep/utils/Shared.hpp>
#include <rxcpp/rx-lite.hpp>

#include <boost/noncopyable.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>

namespace pep::messaging {

/// A sequence of serialized messages that are exchanged asynchronously.
using MessageSequence = rxcpp::observable<std::shared_ptr<std::string>>;

/// \brief A sequence of message sequences that are exchanged asynchronously.
/// \remark The observable-of-observables allows a message processor (such as a network sender) to postpone subscribing
///         to followup sequences ("batches") until the previous sequence has been exhausted. This prevents the processor
///         from being flooded with additional items (from followup batches) while it's still processing earlier items
///         (from a previous batch). The sequential (subscription to and) exhaustion of MessageBatches is implemented in
///         the Scheduler class, which also ensures proper error propagation.
///         For best results, keep each individual MessageSequence ("batch") as small as possible, i.e.
///         prefer increasing numbers of batches over larger batch sizes.
using MessageBatches = rxcpp::observable<MessageSequence>;

/// \brief Creates MessageBatches containing a single message.
/// \param content The single (serialized) message that the batches will consist of.
/// \return MessageBatches containing the specified message.
inline MessageBatches BatchSingleMessage(std::shared_ptr<std::string> content) {
  return rxcpp::observable<>::from(rxcpp::observable<>::from(std::move(content)).as_dynamic());
}

/// \brief Creates MessageBatches containing a single message.
/// \param content The single (serialized) message that the batches will consist of.
/// \return MessageBatches containing the specified message.
inline MessageBatches BatchSingleMessage(std::string content) {
  return BatchSingleMessage(std::make_shared<std::string>(std::move(content)));
}

/// \brief Creates MessageBatches containing a single message.
/// \param content The single message that the batches will consist of.
/// \return MessageBatches containing the specified message (after serialization).
template <class T>
MessageBatches BatchSingleMessage(T content) {
  return BatchSingleMessage(Serialization::ToString(std::move(content)));
}

/// Size for batching messages. Not larger than \c MaxSizeOfMessage.
extern const std::size_t DefaultPageSize;
/// Size for batching messages in Release mode, which may be larger than \c DefaultPageSize in Debug mode.
extern const std::size_t DefaultPageSizeRelease;

/// \brief Creates MessageBatches containing (chunks of) data from the specified stream.
/// \param stream The stream to read data from.
/// \return MessageBatches containing (MessageSequences containing) page-sized blobs extracted from the stream.
/// \remark Data isn't read from the stream (i.e. returned observables don't produce items) until those data are needed/wanted/requested by the caller:
///         1. Caller invokes this function and receives a (single, outer) MessageBatches instance.
///         2. Caller .subscribe()s to the (single, outer) MessageBatches instance, which immediately produces its 1st (inner) MessageSequence (if the stream was nonempty).
///         3. Caller .subscribe()s to the 1st (inner) MessageSequence, which reads the 1st page of data from the stream and emits it as (a shared_ptr to) a string.
///            This exhausts the 1st (inner) MessageSequence, after which the (outer) MessageBatches instance produces a followup (inner) MessageSequence.
///         4. ...Repeat step 3 for (the 2nd and) all followup (inner) MessageSequence instances...
///         5. When the original stream has been fully exhausted, completion/exhaustion is signalled on the (single, outer) MessageBatches instance.
///         Thus, the (outer) MessageBatches instance makes no progress until callers subscribe() to each (inner) MessageSequence instance. The consequence is that
///         (inner) MessageSequence instances must be processed as they are produced (and cannot e.g. be stored for later and/or out-of-order processing).
///         If you're unsure what to do, let .concat() take care of things for you:
///              IStreamToMessageBatches(myStream)
///                .concat()
///                .map([](std::shared_ptr<std::string> page) { return ProcessPage(page); })
///                .subscribe(...);
MessageBatches IStreamToMessageBatches(std::shared_ptr<std::istream> stream);

/// \brief A function that produces the next (serialized) message, or nullptr if there are no more messages.
using MessageProducer = std::function<std::shared_ptr<std::string>()>;

/// \brief Creates MessageBatches containing messages that are produced (one at a time) by a function.
/// \param produce The function that produces messages. Exceptions that it raises are propagated as errors on the
///                (inner) MessageSequence for which the message was being produced.
/// \return MessageBatches that each contain (at most) a single message.
/// \remark Messages are produced lazily, in the same manner as IStreamToMessageBatches: the function is only invoked when
///         an (inner) MessageSequence is subscribed to, and the (outer) MessageBatches only produce a followup
///         MessageSequence after the previous one has been exhausted. The final MessageSequence is empty, since we only
///         know that there are no more messages after the function has said so.
MessageBatches ProducerToMessageBatches(MessageProducer produce);

namespace detail {

inline std::shared_ptr<std::string> ToMessage(std::shared_ptr<std::string> message) {
  if (message == nullptr) {
    throw std::invalid_argument("Cannot send a null message");
  }
  return message;
}

inline std::shared_ptr<std::string> ToMessage(std::string message) {
  return MakeSharedCopy(std::move(message));
}

std::shared_ptr<std::string> ToMessage(auto message) {
  return ToMessage(Serialization::ToString(std::move(message)));
}

/// \brief Iterates over a range, producing a (serialized) message for each element.
template <std::ranges::input_range TRange>
class RangeMessageProducer : public SharedConstructor<RangeMessageProducer<TRange>>, boost::noncopyable {
  friend SharedConstructor<RangeMessageProducer>;

  TRange range_;
  std::optional<std::ranges::iterator_t<TRange>> position_; // Unset until we've started iterating

  explicit RangeMessageProducer(TRange range) : range_(std::move(range)) {}

public:
  /// \brief Produces a message for the next element of the range.
  /// \return The message, or nullptr if the range has been exhausted.
  std::shared_ptr<std::string> next() {
    // Only start or advance the iterator when a message is requested, so that (e.g.) generators do their work lazily
    if (position_.has_value()) {
      ++*position_;
    }
    else {
      position_.emplace(std::ranges::begin(range_));
    }

    if (*position_ == std::ranges::end(range_)) {
      return nullptr;
    }
    return ToMessage(std::ranges::iter_move(*position_));
  }
};

}

/// \brief Creates MessageBatches containing the elements of a range as messages.
/// \tparam TRange An input range of either serialized messages (std::shared_ptr<std::string> or std::string), or of
///                instances of a type that can be serialized (using the Serialization class).
/// \param range The range whose elements to send. Elements are moved out of it as they're needed.
/// \return MessageBatches that each contain (at most) a single message: see ProducerToMessageBatches.
/// \remark The range is iterated as messages are needed (lazily, in between event loop iterations), so any data that it
///         refers to must remain valid until the MessageBatches have been exhausted. A pep::Generator can e.g. keep such
///         data alive by holding a std::shared_ptr to it in its coroutine frame.
/// \remark Each element is sent as a separate message, so its (serialized) size must not exceed MaxSizeOfMessage.
template <std::ranges::input_range TRange>
MessageBatches RangeToMessageBatches(TRange range) {
  auto producer = detail::RangeMessageProducer<TRange>::Create(std::move(range));
  return ProducerToMessageBatches([producer = std::move(producer)] { return producer->next(); });
}

}
