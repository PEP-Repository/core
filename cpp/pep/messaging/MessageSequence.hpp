#pragma once

#include <pep/serialization/Serialization.hpp>
#include <rxcpp/rx-lite.hpp>

namespace pep::messaging {

/**
 * @brief A sequence of serialized messages that are exchanged asynchronously.
 */
using MessageSequence = rxcpp::observable<std::shared_ptr<std::string>>;

/**
 * @brief A sequence of message sequences that are exchanged asynchronously.
 * @remark The observable-of-observables allows a message processor (such as a network sender) to postpone subscribing
 *         to followup sequences ("batches") until the previous sequence has been exhausted. This prevents the processor
 *         from being flooded with additional items (from followup batches) while it's still processing earlier items
 *         (from a previous batch). The sequential (subscription to and) exhaustion of MessageBatches is implemented in
 *         the Scheduler class, which also ensures proper error propagation.
 *         For best results, keep each individual MessageSequence ("batch") as small as possible, i.e.
 *         prefer increasing numbers of batches over larger batch sizes.
 */
using MessageBatches = rxcpp::observable<MessageSequence>;

/**
 * @brief Creates MessageBatches containing a single message.
 * @param content The single (serialized) message that the batches will consist of.
 * @return MessageBatches containing the specified message.
 */
inline MessageBatches BatchSingleMessage(std::shared_ptr<std::string> content) {
  return rxcpp::observable<>::from(rxcpp::observable<>::from(std::move(content)).as_dynamic());
}

/**
 * @brief Creates MessageBatches containing a single message.
 * @param content The single (serialized) message that the batches will consist of.
 * @return MessageBatches containing the specified message.
 */
inline MessageBatches BatchSingleMessage(std::string content) {
  return BatchSingleMessage(std::make_shared<std::string>(std::move(content)));
}

/**
 * @brief Creates MessageBatches containing a single message.
 * @param content The single message that the batches will consist of.
 * @return MessageBatches containing the specified message (after serialization).
 */
template <class T>
MessageBatches BatchSingleMessage(T content) {
  return BatchSingleMessage(Serialization::ToString(std::move(content)));
}

MessageBatches IStreamToMessageBatches(std::shared_ptr<std::istream> stream);

}
