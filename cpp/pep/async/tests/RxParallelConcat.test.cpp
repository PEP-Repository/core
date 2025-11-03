#include <gtest/gtest.h>

#include <rxcpp/operators/rx-reduce.hpp>
#include <rxcpp/operators/rx-take.hpp>

#include <pep/async/RxMoveIterate.hpp>
#include <pep/async/RxParallelConcat.hpp>
#include <pep/async/OnAsio.hpp>
#include <pep/async/CreateObservable.hpp>
#include <pep/async/tests/RxTestUtils.hpp>

#include <boost/asio/io_context.hpp>

using namespace pep;

namespace {

TEST(RxParallelConcat, test_test_auto_unsubscribe)
{
  std::vector<int> values = {0,1,2,3};
  rxcpp::composite_subscription subscription;
  RxMoveIterate(values).subscribe(subscription,
      [&subscription](int i){
        EXPECT_TRUE(subscription.is_subscribed());
      },
      [&subscription](){
        EXPECT_TRUE(subscription.is_subscribed());
      }
  );
  EXPECT_FALSE(subscription.is_subscribed());
}

TEST(RxParallelConcat, test_test_interval)
{
  // check that observable.subscribe is blocking when no
  // subscribe_on is used.
  auto period = std::chrono::milliseconds(1);
  auto values = rxcpp::observable<>::interval(period);

  bool done = false;

  values.take(4).count()
    .subscribe(
      [](int count){ // on_next
        EXPECT_EQ(count, 4);
      },
      [&done](){ // on_complete
        done = true;
      });

  EXPECT_TRUE(done);

  // But: observable.subscribe is _not_ blocking when subscribe_on is used:
  boost::asio::io_context io_context;

  values = rxcpp::observable<>::interval(period);
  done = false;
  values.take(4).count().subscribe_on(observe_on_asio(io_context))
    .subscribe(
      [](int count){ // on_next
        EXPECT_EQ(count, 4);
      },
      [&done](){ // on_complete
        done = true;
      });

  EXPECT_FALSE(done);

  io_context.run();

  EXPECT_TRUE(done);
}


TEST(RxParallelConcat, CachingSubscriber_I)
{
  detail::CachingSubscriber<int> cache;

  ASSERT_FALSE(cache.item_ready());
  ASSERT_FALSE(cache.end_ready());

  cache.on_next(1);
  ASSERT_TRUE(cache.item_ready());
  ASSERT_FALSE(cache.end_ready());

  cache.on_next(2);
  ASSERT_TRUE(cache.item_ready());
  ASSERT_FALSE(cache.end_ready());

  ASSERT_EQ(cache.pop(), 1);
  ASSERT_TRUE(cache.item_ready());
  ASSERT_FALSE(cache.end_ready());

  ASSERT_EQ(cache.pop(), 2);
  ASSERT_FALSE(cache.item_ready());
  ASSERT_FALSE(cache.end_ready());

  cache.on_next(3);
  cache.on_completed();
  ASSERT_TRUE(cache.item_ready());
  ASSERT_FALSE(cache.end_ready());

  cache.pop();
  ASSERT_FALSE(cache.item_ready());
  ASSERT_TRUE(cache.end_ready());

  bool completed = false;

  cache.take_one(
    [](int i){ // on_next
      ASSERT_TRUE(false);
    },
    [](std::exception_ptr ep){ // on_error
      ASSERT_TRUE(false);
    },
    [&completed]{ // on_completed
      ASSERT_FALSE(completed);
      completed = true;
    }
  );
  ASSERT_TRUE(completed);
}

TEST(RxParallelConcat, CachingSubscriber_II)
{
  detail::CachingSubscriber<int> cache;

  ASSERT_FALSE(cache.item_ready());
  ASSERT_FALSE(cache.end_ready());

  cache.on_next(1);

  ASSERT_TRUE(cache.item_ready());
  ASSERT_FALSE(cache.end_ready());

  cache.on_error(std::make_exception_ptr(std::runtime_error("foobar")));

  ASSERT_TRUE(cache.item_ready());
  ASSERT_FALSE(cache.end_ready());

  bool had_one = false;

  cache.take_one(
    [&had_one](int i){ // on_next
      ASSERT_FALSE(had_one);
      had_one = true;
      ASSERT_EQ(i, 1);
    },
    [](std::exception_ptr ep){ // on_error
      ASSERT_TRUE(false);
    },
    []{ // on_completed
      ASSERT_TRUE(false);
    }
  );
  ASSERT_TRUE(had_one);

  ASSERT_FALSE(cache.item_ready());
  ASSERT_TRUE(cache.end_ready());

  bool had_on_error = false;
  cache.take_one(
    [](int i){ // on_next
      ASSERT_FALSE(true);
    },
    [&had_on_error](std::exception_ptr ep){ // on_error
      ASSERT_FALSE(had_on_error);
      had_on_error = true;
    },
    []{ // on_completed
      ASSERT_TRUE(false);
    }
  );
  ASSERT_TRUE(had_on_error);
}


template<typename T>
static rxcpp::observable<T> MakeObservable(std::optional<rxcpp::subscriber<T>>& sub) {
  // We use an optional, because rxcpp::subscriber<T> has a private constructor.
  return CreateObservable<T>(
    [&sub](rxcpp::subscriber<T> s){ sub = s; });
}


TEST(RxParallelConcat, CachingObservable)
{
  std::optional<rxcpp::subscriber<int>> sub;
  rxcpp::observable<int> obs = MakeObservable(sub);
  detail::RxParallelConcatContext<int>::CachingObservable co(obs);

  ASSERT_TRUE(sub.has_value());

  sub->on_next(1);

  ASSERT_TRUE(co.item_cache.item_ready());
  ASSERT_EQ(co.item_cache.pop(), 1);

  bool had_one = false;
  bool had_completed = false;

  rxcpp::composite_subscription sption = co.hijack(rxcpp::make_subscriber<int>(
      [&had_one, &had_completed](int i) {
        ASSERT_EQ(i, 2);
        ASSERT_FALSE(had_one);
        ASSERT_FALSE(had_completed);
        had_one = true;
      },
      [](std::exception_ptr ep) {
        ASSERT_FALSE(true);
      },
      [&had_one, &had_completed]() {
        ASSERT_TRUE(had_one);
        ASSERT_FALSE(had_completed);
        had_completed = true;
      }));

  ASSERT_TRUE(sption.is_subscribed());

  ASSERT_FALSE(co.subscription.has_value());

  ASSERT_FALSE(had_one || had_completed);

  sub->on_next(2);

  ASSERT_TRUE(had_one && !had_completed);
  ASSERT_FALSE(co.item_cache.item_ready());
  ASSERT_FALSE(co.item_cache.end_ready());

  sub->on_completed();

  ASSERT_TRUE(had_one && had_completed);
  ASSERT_FALSE(co.item_cache.item_ready());
  ASSERT_FALSE(co.item_cache.end_ready());
  ASSERT_FALSE(sption.is_subscribed());
}


TEST(RxParallelConcat, soundness)
{
  struct Context {
    std::optional<rxcpp::subscriber<int>> sub1, sub2, sub3;
    std::optional<rxcpp::subscriber<rxcpp::observable<int>>> sub;

    rxcpp::observable<int> obs1 = MakeObservable(sub1),
                           obs2 = MakeObservable(sub2),
                           obs3 = MakeObservable(sub3);
    rxcpp::observable<rxcpp::observable<int>> obs = MakeObservable(sub);

    std::vector<int> results;
    std::optional<std::exception_ptr> exception;
    bool completed = false;

    Context(){
      this->obs.op(RxParallelConcat(2)).subscribe(
        [this](int i){
          EXPECT_FALSE(this->completed) << "i=" << i;
          EXPECT_FALSE(this->exception.has_value());
          this->results.push_back(i);
        },
        [this](std::exception_ptr ep){
          EXPECT_FALSE(this->completed);
          EXPECT_FALSE(this->exception.has_value());
          this->exception = ep;
        },
        [this]{
          EXPECT_FALSE(this->completed);
          EXPECT_FALSE(this->exception.has_value());
          this->completed = true;
        });

      EXPECT_TRUE(this->sub.has_value());
    }
  };

  std::exception_ptr ep = std::make_exception_ptr(
      std::runtime_error("some error"));

  { // Scenario I
    Context c;
    c.sub->on_next(c.obs1);
    c.sub->on_next(c.obs2);
    c.sub->on_next(c.obs3);
    c.sub->on_completed();
    c.sub->on_error(ep); // has no effect, but curiously raises no error either
    c.sub->on_next(c.obs1);

    EXPECT_TRUE(c.sub1.has_value());
    EXPECT_TRUE(c.sub2.has_value());
    EXPECT_FALSE(c.sub3.has_value());

    c.sub1->on_completed();

    ASSERT_TRUE(c.sub3.has_value());

    c.sub3->on_next(2); c.sub3->on_completed();
    c.sub2->on_next(1); c.sub2->on_completed();


    EXPECT_TRUE(c.completed);
    EXPECT_EQ(c.results, std::vector<int>({1,2}));
  }

  { // Scenario II - error in obs2
    Context c;
    c.sub->on_next(c.obs1);
    c.sub->on_next(c.obs2);
    c.sub->on_next(c.obs3);
    c.sub->on_completed();

    EXPECT_TRUE(c.sub1.has_value());
    EXPECT_TRUE(c.sub2.has_value());
    EXPECT_FALSE(c.sub3.has_value());

    c.sub2->on_next(2);
    c.sub1->on_next(1);
    c.sub2->on_error(ep);
    c.sub1->on_completed();

    EXPECT_TRUE(c.exception.has_value());
    EXPECT_EQ(c.results, std::vector<int>({1,2}));
  }

  { // Scenario III - error in obs1
    Context c;
    c.sub->on_next(c.obs1);
    c.sub->on_next(c.obs2);
    c.sub->on_next(c.obs3);
    c.sub->on_completed();

    EXPECT_TRUE(c.sub1.has_value());
    EXPECT_TRUE(c.sub2.has_value());
    EXPECT_FALSE(c.sub3.has_value());

    c.sub2->on_next(2);
    c.sub1->on_next(1);
    c.sub1->on_error(ep);
    c.sub2->on_completed();

    EXPECT_TRUE(c.exception.has_value());
    EXPECT_EQ(c.results, std::vector<int>({1}));
  }

  { // Scenario IV - error in obs
    Context c;
    c.sub->on_next(c.obs1);
    c.sub->on_next(c.obs2);
    c.sub->on_error(ep);

    EXPECT_TRUE(c.sub1.has_value());
    EXPECT_TRUE(c.sub2.has_value());

    c.sub1->on_next(1);
    c.sub2->on_next(3);
    c.sub1->on_next(2);
    c.sub2->on_next(4);
    c.sub2->on_completed();
    c.sub1->on_completed();

    EXPECT_TRUE(c.exception.has_value());
    EXPECT_EQ(c.results, std::vector<int>({1,2,3,4}));
  }

  { // Scenario V - late obs.on_completed
    Context c;
    c.sub->on_next(c.obs1);
    EXPECT_TRUE(c.sub1.has_value());
    c.sub1->on_completed();

    c.sub->on_next(c.obs2);
    EXPECT_TRUE(c.sub2.has_value());
    c.sub2->on_completed();

    c.sub->on_next(c.obs3);
    EXPECT_TRUE(c.sub3.has_value());
    c.sub3->on_completed();

    EXPECT_FALSE(c.completed);
    c.sub->on_completed();
    EXPECT_TRUE(c.completed);

    EXPECT_EQ(c.results, std::vector<int>({}));
  }

  { // Scenario VI - late obs.on_error
    Context c;
    c.sub->on_next(c.obs1);
    EXPECT_TRUE(c.sub1.has_value());
    c.sub1->on_completed();

    c.sub->on_next(c.obs2);
    EXPECT_TRUE(c.sub2.has_value());
    c.sub2->on_completed();

    c.sub->on_next(c.obs3);
    EXPECT_TRUE(c.sub3.has_value());
    c.sub3->on_completed();

    EXPECT_FALSE(c.exception.has_value());
    c.sub->on_error(ep);
    EXPECT_TRUE(c.exception.has_value());

    EXPECT_EQ(c.results, std::vector<int>({}));
  }

  { // Scenario VII - in sequence
    Context c;
    c.sub->on_next(c.obs1);
    EXPECT_TRUE(c.sub1.has_value());
    c.sub1->on_next(1);
    c.sub1->on_completed();

    c.sub->on_next(c.obs2);
    EXPECT_TRUE(c.sub2.has_value());
    c.sub2->on_next(2);
    c.sub2->on_completed();

    c.sub->on_next(c.obs3);
    EXPECT_TRUE(c.sub3.has_value());
    c.sub3->on_next(3);
    c.sub3->on_completed();
    c.sub->on_completed();

    EXPECT_TRUE(c.completed);

    EXPECT_EQ(c.results, std::vector<int>({1,2,3}));
  }

}

}
