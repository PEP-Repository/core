#include <pep/async/RxTimeout.hpp>
#include <boost/asio/steady_timer.hpp>

namespace pep {

rxcpp::observable<FakeVoid> RxAsioTimer(const std::chrono::milliseconds& duration, boost::asio::io_context& io_context, rxcpp::observe_on_one_worker observe_on) {
  /* A steady_timer is monotonic, eliminating some (possible) problems with
   * the non-monotonic deadline_timer. See e.g. https://stackoverflow.com/a/14848254 .
   *
   * Also it has been observed that a boost::asio::deadline_timer sometimes expires faster
   * than the specified duration, measured as the difference between two calls to
   * std::chrono::steady_clock::now(). Since a boost::asio::steady_timer is based on that
   * same std::chrono::steady_clock, it will (presumably) behave consistently with such
   * measurements (as e.g. used in the RxTimeout.test.cpp source).
   */
  auto implementor = std::make_shared<boost::asio::steady_timer>(io_context);
  auto expire_after = duration_cast<boost::asio::steady_timer::duration>(duration);

  return CreateObservable<FakeVoid>([implementor, expire_after](rxcpp::subscriber<FakeVoid> subscriber) {
    implementor->expires_after(expire_after);
    implementor->async_wait([subscriber, implementor /* keep implementor alive until it expires or is cancelled */](const boost::system::error_code& error) {
      if (error) {
        assert(error == boost::asio::error::operation_aborted);
        return; // Timer was cancelled: don't emit to subscriber
      }

      subscriber.on_next(FakeVoid{});
      subscriber.on_completed();
      });

    // Attach a lambda that's invoked when the subscriber unsubscribes: see https://github.com/ReactiveX/RxCpp/issues/517#issuecomment-555618051
    subscriber.add([implementor]() {
      implementor->cancel(); // Don't keep the io_context busy if no one is interested in the timer anymore
      });
    })
    .subscribe_on(observe_on_asio(io_context))
    .observe_on(observe_on);
}

}
