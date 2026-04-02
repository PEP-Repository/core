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

extern const uint64_t DEFAULT_PAGE_SIZE;

/**
 * @brief Creates MessageBatches containing (chunks of) data from the specified stream.
 * @param stream The stream to read data from.
 * @return MessageBatches containing (MessageSequences containing) page-sized blobs extracted from the stream.
 * @remark Data isn't read from the stream (i.e. returned observables don't produce items) until those data are needed/wanted/requested by the caller:
 *         1. Caller invokes this function and receives a (single, outer) MessageBatches instance.
 *         2. Caller .subscribe()s to the (single, outer) MessageBatches instance, which immediately produces its 1st (inner) MessageSequence (if the stream was nonempty).
 *         3. Caller .subscribe()s to the 1st (inner) MessageSequence, which reads the 1st page of data from the stream and emits it as (a shared_ptr to) a string.
 *            This exhausts the 1st (inner) MessageSequence, after which the (outer) MessageBatches instance produces a followup (inner) MessageSequence.
 *         4. ...Repeat step 3 for (the 2nd and) all followup (inner) MessageSequence instances...
 *         5. When the original stream has been fully exhausted, completion/exhaustion is signalled on the (single, outer) MessageBatches instance.
 *         Thus, the (outer) MessageBatches instance makes no progress until callers subscribe() to each (inner) MessageSequence instance. The consequence is that
 *         (inner) MessageSequence instances must be processed as they are produced (and cannot e.g. be stored for later and/or out-of-order processing).
 *         If you're unsure what to do, let .concat_map take care of things for you:
 *              IStreamToMessageBatches(myStream)
 *                .concat_map([](MessageSequence batch) { return batch; })
 *                .map([](std::shared_ptr<std::string> page) { return ProcessPage(page); })
 *                .subscribe(...);
 */
MessageBatches IStreamToMessageBatches(std::shared_ptr<std::istream> stream);

}
