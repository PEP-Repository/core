#include <pep/messaging/Requestor.hpp>
#include <pep/utils/Log.hpp>
#include <pep/serialization/MessageMagic.hpp>
#include <pep/serialization/Error.hpp>
#include <pep/async/OnAsio.hpp>
#include <pep/utils/Defer.hpp>
#include <pep/utils/Exceptions.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>

namespace pep::messaging {

namespace {

const std::string LOG_TAG = "Requestor";

}

Requestor::Entry::Entry(std::shared_ptr<std::string> message,
  std::optional<MessageBatches> tail,
  bool resendable,
  rxcpp::subscriber<std::string> subscriber)
  : message(message), tail(tail), resendable(resendable), subscriber(subscriber) {
}

Requestor::Requestor(boost::asio::io_context& io_context, Scheduler& scheduler)
  : mIoContext(io_context), mScheduler(scheduler) {
}

Requestor::~Requestor() noexcept {
  if (mEntries.size() != 0) {
    LOG(LOG_TAG, severity_level::error)
      << "outstanding requests list is not empty:";
    for (const auto& kv : mEntries) {
      std::string msgType = "(too short)";
      auto msg = kv.second.message;
      if (msg->size() >= sizeof(MessageMagic))
        msgType = DescribeMessageMagic(*msg);
      LOG(LOG_TAG, severity_level::error)
        << " streamid " << kv.first
        << " " << msgType;
    }
    assert(false);
  }
}

StreamId Requestor::getNewRequestStreamId() {
  // use the previous request ID to start looking for a new one
  auto result = mPreviousRequestStreamId;

  do {
    // ensure that ID differs from the previously generated one
    result = StreamId::MakeNext(result);
  } while (mEntries.find(result) != mEntries.end()); // ensure that we don't recycle IDs of requests that we haven't received a reply to

  // ensure that a future call doesn't produce this ID again
  return mPreviousRequestStreamId = result;
}

rxcpp::observable<std::string> Requestor::send(std::shared_ptr<std::string> request, std::optional<MessageBatches> tail, bool immediately, bool resend) {
  assert(immediately || resend);

  return CreateObservable<std::string>([self = SharedFrom(*this), request, tail, immediately, resend](rxcpp::subscriber<std::string> s) {
    auto streamId = self->getNewRequestStreamId();

    auto emplacement = self->mEntries.emplace(streamId, Entry(request, tail, resend, s));
    assert(emplacement.second);

    if (immediately) {
      self->schedule(*emplacement.first);
    }
    })
    .subscribe_on(observe_on_asio(mIoContext));
}

void Requestor::processResponse(const std::string& recipient, const StreamId& streamId, const Flags& flags, std::string body) {
  auto it = mEntries.find(streamId);
  if (it == mEntries.end()) {
    LOG(LOG_TAG, warning) << "received response for non existent stream: " << streamId << " (to " << recipient << ")";
    return;
  }

  // got response, send it back using the rx subscriber
  auto subscriber = it->second.subscriber;

  bool close = flags.close();
  bool error = flags.error();
  bool payload = flags.payload();

  if (error || close) {
    mEntries.erase(it);
  }
  PEP_DEFER(
    if (error || close) {
      LOG(LOG_TAG, severity_level::verbose)
        << "Closed stream " << streamId
        << " (to " << recipient << ")";
    });

  if (error) {
    auto err = Error::ReconstructIfDeserializable(body);
    if (err == nullptr) {
      // Backward compatible: if no Error instance could be deserialized, report on an empty instance
      err = std::make_exception_ptr(Error(std::string()));
    }
    LOG(LOG_TAG, severity_level::error)
      << "Received an error! (stream id " << streamId
      << " to " << recipient << "): "
      << GetExceptionMessage(err);
    subscriber.on_error(err);
  } else {
    if (payload)
      subscriber.on_next(std::move(body));
    if (close)
      subscriber.on_completed();
  }
}

void Requestor::purge(bool resendable) {
  // remove requests that should not be re-sent
  for (auto it = mEntries.begin(); it != mEntries.end(); /* sic */) {
    // confused? See the example of:
    //  https://en.cppreference.com/w/cpp/container/map/erase

    auto& request = it->second;

    if (resendable || !request.resendable) {
      // remove this request from the list

      // notify caller of the failure, but not directly!, since this might
      // affect the this->requests map while we're looping over it
      //
      // it might be more consistent to put an "observe_on(observe_on_asio(..))"
      // on the observable returned by sendRequest, but I do not oversee the
      // all the consequences that might have
      post(mIoContext.get_executor(),
        [subscriber = std::move(request.subscriber)] {
          subscriber.on_error(std::make_exception_ptr(
            Error("Aborting multi-message request")));
        });

      it = mEntries.erase(it); // Position "it" on the next item, i.e. the one after the one that we erased
    } else {
      it++; // Move "it" to the next item
    }
  }
}

void Requestor::resend() {
  for (auto& entry : mEntries) {
    auto& request = entry.second;
    if (request.resendable) {
      assert(request.message->size() != 0U);
      this->schedule(entry);
    }
  }
}

void Requestor::schedule(decltype(mEntries)::value_type& entry) {
  const auto& streamId = entry.first;
  auto& request = entry.second;
  if (request.tail.has_value()) {
    request.resendable = false; // currently we cannot re-generate tail messages already sent, see #1225
  }
  mScheduler.push(streamId, request.message, request.tail);
}

}
