#include <gtest/gtest.h>
#include <pep/async/CreateObservable.hpp>
#include <pep/async/RxIterate.hpp>
#include <pep/messaging/Scheduler.hpp>
#include <pep/serialization/Error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <cmath>
#include <numeric>

using namespace std::literals;

namespace {

class FakeSender {
public:
  struct StreamOutput {
    size_t items = 0U;
    std::exception_ptr exception;
    bool closed = false;
  };

private:
  std::shared_ptr<pep::messaging::Scheduler> scheduler_;
  std::map<pep::messaging::MessageId, StreamOutput> streams_;
  pep::EventSubscription availableSubscription_;
  pep::EventSubscription errorSubscription_;

  void ensureSend() {
    while (scheduler_->available()) {
      auto outgoing = scheduler_->pop();
      auto& stream = streams_[outgoing.properties.messageId()];

      ASSERT_FALSE(stream.closed) << "Scheduler produced message after CLOSE";
      if (outgoing.properties.flags().has(pep::messaging::Flags::Close)) {
        stream.closed = true;
      }
      ++stream.items;
    }
  }

  void handleAvailable() {
    this->ensureSend();
  }

  void handleError(const pep::messaging::MessageId& id, std::exception_ptr error) {
    ASSERT_NE(error, nullptr) << "Scheduler sent notification of a NULL error";
    ASSERT_FALSE(id.type() == pep::messaging::MessageType::Request && pep::Error::IsSerializable(error)) << "Request streams shouldn't produce exceptions of type Error (or derived)";

    ASSERT_EQ(streams_[id].exception, nullptr) << "Scheduler sent multiple error notifications";
    streams_[id].exception = error;
  }

public:
  explicit FakeSender(std::shared_ptr<pep::messaging::Scheduler> scheduler)
    : scheduler_(scheduler) {
    availableSubscription_ = scheduler_->onAvailable.subscribe([this]() {this->handleAvailable(); });
    errorSubscription_ = scheduler_->onError.subscribe([this](const pep::messaging::MessageId& id, std::exception_ptr error) {this->handleError(id, error); });
  }

  size_t count() const noexcept { return std::accumulate(streams_.cbegin(), streams_.cend(), size_t{}, [](size_t total, const auto& pair) {return total + pair.second.items; }); }
  bool closed() const noexcept { return std::all_of(streams_.cbegin(), streams_.cend(), [](const auto& pair) { return pair.second.closed; }); }
  bool error() const noexcept { return std::any_of(streams_.cbegin(), streams_.cend(), [](const auto& pair) { return pair.second.exception != nullptr; }); }
};


size_t ItemCount(size_t batchIndex) {
  return static_cast<size_t>(std::pow(2, batchIndex + 1U));
}

class Emitter : public std::enable_shared_from_this<Emitter>, public pep::SharedConstructor<Emitter> {
  friend class pep::SharedConstructor<Emitter>;

private:
  boost::asio::steady_timer timer_;
  rxcpp::subscriber<std::shared_ptr<std::string>> subscriber_;
  size_t& exhaustCount_;
  std::string prefix_;
  size_t total_;
  size_t index_ = 0U;

  explicit Emitter(boost::asio::io_context& ioContext, rxcpp::subscriber<std::shared_ptr<std::string>> subscriber, size_t& exhaustCount, size_t index)
    : timer_(ioContext), subscriber_(subscriber), exhaustCount_(exhaustCount), prefix_(std::to_string(index) + '.'), total_(ItemCount(index)) {}

  void handleTimerExpired(const boost::system::error_code& error) {
    if (error && error != boost::asio::error::operation_aborted) {
      throw boost::system::system_error(error);
    }

    subscriber_.on_next(pep::MakeSharedCopy(prefix_ + std::to_string(index_++)));
    if (this->finished()) {
      ++exhaustCount_;
      subscriber_.on_completed();
    } else {
      this->scheduleNext();
    }
  }

  void scheduleNext() {
    if (this->finished()) {
      throw std::runtime_error("Can't schedule a next item from a finished emitter");
    }
    timer_.expires_after(10ms);
    timer_.async_wait([self = SharedFrom(*this)](const boost::system::error_code& error) { self->handleTimerExpired(error); });
  }

  bool finished() const noexcept { return index_ >= total_; }

public:
  static pep::messaging::MessageSequence MakeBatch(boost::asio::io_context& ioContext, size_t& exhaustCount, size_t index) {
    return pep::CreateObservable<std::shared_ptr<std::string>>([&ioContext, &exhaustCount, index](rxcpp::subscriber<std::shared_ptr<std::string>> subscriber) {
      ASSERT_EQ(exhaustCount, index) << "Batch " << index << " was subscribed when " << exhaustCount << " previous one(s) were exhausted";
      auto emitter = Emitter::Create(ioContext, subscriber, exhaustCount, index);
      emitter->scheduleNext();
      });
  }
};

struct Stream {
  pep::messaging::StreamId id;
  std::unique_ptr<size_t> exhausted = std::make_unique<size_t>(0U);
  std::vector<pep::messaging::MessageSequence> batches;

  Stream(const Stream&) = delete;
  Stream& operator=(const Stream&) = delete;

  Stream(Stream&&) = default;
  Stream& operator=(Stream&&) = default;

  Stream(boost::asio::io_context& ioContext, pep::messaging::StreamId& previousStreamId, size_t batchCount)
    : id(previousStreamId = pep::messaging::StreamId::MakeNext(previousStreamId)) {
    while (batches.size() != batchCount) {
      batches.emplace_back(Emitter::MakeBatch(ioContext, *exhausted, batches.size()));
    }
  }

  size_t itemCount() const { return ItemCount(batches.size()) - 1U; } // Powers of 2: the next batch would have held one item more than all previous batches combined
};


void TestStreams(const std::vector<size_t> sizes) {
  boost::asio::io_context ioContext;
  auto streamId = pep::messaging::StreamId::BeforeFirst();

  std::vector<Stream> streams;
  for (auto size : sizes) {
    streams.emplace_back(ioContext, streamId, size);
  }

  size_t items = std::accumulate(streams.cbegin(), streams.cend(), size_t{}, [](size_t total, const Stream& stream) {return total + stream.itemCount(); });

  auto scheduler = pep::messaging::Scheduler::Create(ioContext);
  FakeSender sender(scheduler);

  for (const auto& stream : streams) {
    scheduler->push(stream.id, pep::RxIterate(stream.batches));
  }
  ASSERT_NO_FATAL_FAILURE(ioContext.run());

  ASSERT_FALSE(sender.error()) << "Error occurred during scheduling";
  for (const auto& stream : streams) {
    ASSERT_EQ(stream.batches.size(), *stream.exhausted) << "Batches weren't exhausted";
  }
  ASSERT_GE(sender.count(), items) << "Stream items weren't exhausted";
  ASSERT_LE(sender.count(), items + 1U) << "Scheduler produced more than just stream messages plus a(n optional) final close message";
  ASSERT_TRUE(sender.closed()) << "Scheduler didn't produce a CLOSE message";

}


TEST(Scheduler, WithoutFailure) {
  TestStreams({ 6U });
  TestStreams({ 3U, 5U, 7U });
}


pep::messaging::MessageBatches TopLevelFailureDuringItemEmission() {
  auto batches = pep::CreateObservable<pep::messaging::MessageSequence>([](rxcpp::subscriber<pep::messaging::MessageSequence> batchesSubscriber) {

    // We'll produce a single batch...
    auto batch = pep::CreateObservable<std::shared_ptr<std::string>>([batchesSubscriber](rxcpp::subscriber<std::shared_ptr<std::string>> itemsSubscriber) {
      itemsSubscriber.on_next(std::make_shared<std::string>("One"));
      itemsSubscriber.on_next(std::make_shared<std::string>("Two"));

      // ... then encounter a failure on the parent ("batches") observable while the child is emitting items
      batchesSubscriber.on_error(std::make_exception_ptr(std::runtime_error("Failure in batches (parent) observable")));
      ASSERT_FALSE(batchesSubscriber.is_subscribed()) << "Batches subscriber shouldn't be subscribed anymore after failure";
      ASSERT_TRUE(itemsSubscriber.is_subscribed()) << "Items subscriber should keep reading after batches failure";

      itemsSubscriber.on_next(std::make_shared<std::string>("Three"));
      itemsSubscriber.on_next(std::make_shared<std::string>("Four"));
      itemsSubscriber.on_error(std::make_exception_ptr(std::runtime_error("Failure in batch (child) observable")));
      });

    batchesSubscriber.on_next(batch);
    // batchesSubscriber.on_error is invoked during exhaustion of the child ("batch") observable
    });

  return batches;
}

TEST(Scheduler, HandlesTopLevelFailureDuringItemEmission) {
  boost::asio::io_context ioContext;
  auto streamId = pep::messaging::StreamId::BeforeFirst();

  auto scheduler = pep::messaging::Scheduler::Create(ioContext);
  FakeSender sender(scheduler);
  scheduler->push(streamId = pep::messaging::StreamId::MakeNext(streamId), TopLevelFailureDuringItemEmission());

  ASSERT_NO_FATAL_FAILURE(ioContext.run());

  ASSERT_TRUE(sender.error()) << "Error occurred during scheduling";
  ASSERT_GE(sender.count(), 4U) << "Stream items weren't exhausted";
  ASSERT_LE(sender.count(), 6U) << "Scheduler produced more than just stream messages plus error message plus a(n optional) final close message";
  ASSERT_TRUE(sender.closed()) << "Scheduler didn't produce a CLOSE message";
}

}
