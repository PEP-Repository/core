#pragma once

#include <pep/async/ActivityMonitor.hpp>
#include <pep/async/CreateObservable.hpp>
#include <pep/utils/Exceptions.hpp>

namespace pep {

/*! \brief Monitors an observable, logging a warning if it shows no activity.
 * \tparam TItem The item type emitted by the source observable.
 * \tparam SourceOperator The source operator type included in the observable type.
 * \param io_context The I/O context to associate with the timer that monitors the observable.
 * \param description A description of the job being monitored.
 * \param maxInactive The maximum amount of time between emissions.
 * \param items The observable emitting the items.
 * \return An observable emitting the original items.
 */
template <typename TItem, typename SourceOperator>
rxcpp::observable<TItem> RxEnsureProgress(boost::asio::io_context& io_context, const std::string& description, const std::chrono::milliseconds maxInactive, rxcpp::observable<TItem, SourceOperator> items) {
  // Don't expect (possibly cold) observables to start work immediately, but wait until a subscriber requests items
  return CreateObservable<TItem>([&io_context, description, maxInactive, items](rxcpp::subscriber<TItem> subscriber) {
    auto monitor = ActivityMonitor::Create(io_context, description, maxInactive);
    items.subscribe(
      [subscriber, monitor](const TItem& item) {
        monitor->activityOccurred("emitted item");
        subscriber.on_next(item);
      },
      [subscriber, monitor /* just keep "monitor" alive until done */](std::exception_ptr ep) {
        subscriber.on_error(ep);
      },
      [subscriber, monitor /* just keep "monitor" alive until done */]() {
        subscriber.on_completed();
      }
      );
    });
}

/*! \brief Monitors an observable, logging a warning if it shows no activity (for the default-allotted time).
 * \tparam TItem The item type emitted by the source observable.
 * \tparam SourceOperator The source operator type included in the observable type.
 * \param io_context The I/O context to associate with the timer that monitors the observable.
 * \param description A description of the job being monitored.
 * \param items The observable emitting the items.
 * \return An observable emitting the original items.
 */
template <typename TItem, typename SourceOperator>
rxcpp::observable<TItem> RxEnsureProgress(boost::asio::io_context& io_context, const std::string& description, rxcpp::observable<TItem, SourceOperator> items) {
  return RxEnsureProgress(io_context, description, ActivityMonitor::DEFAULT_MAX_INACTIVE, items);
}

/*! \brief Monitors an observable, logging a warning if it shows no activity (for the default-allotted time).
 * \tparam CreateSource A function-like object that accepts (a shared_ptr to) an ActivityMonitor and returns an rxcpp::observable<> whose activity is to be monitored.
 * \param io_context The I/O context to associate with the timer that monitors the observable.
 * \param description A description of the job being monitored.
 * \param maxInactive The maximum duration of non-activity.
 * \param createSource A function-like object that produces the source observable.
 * \return An observable emitting the source observable's items.
 * 
 * \remark Allows .op(RxRecordActivity(monitor)) to be interspersed in the factory function's RX pipeline.
 */
template <typename CreateSource>
rxcpp::observable<typename decltype(std::declval<CreateSource>()(std::declval<std::shared_ptr<ActivityMonitor>>()))::value_type>
RxEnsureProgress(boost::asio::io_context& io_context, const std::string& description, const std::chrono::milliseconds maxInactive, const CreateSource& createSource) {
  using TItem = typename decltype(std::declval<CreateSource>()(std::declval<std::shared_ptr<ActivityMonitor>>()))::value_type;

  // Don't start monitoring until a subscriber requests items...
  return CreateObservable<TItem>([&io_context, description, maxInactive, createSource](rxcpp::subscriber<TItem> subscriber) {
    auto monitor = ActivityMonitor::Create(io_context, description, maxInactive);
    // ... and create the source only at that time, since we can then pass our ActivityMonitor into the factory function
    createSource(monitor).subscribe(
      [subscriber, monitor](const TItem& item) {
        monitor->activityOccurred("emitted item");
        subscriber.on_next(item);
      },
      [subscriber, monitor /* just keep "monitor" alive until done */](std::exception_ptr ep) {
        subscriber.on_error(ep);
      },
      [subscriber, monitor /* just keep "monitor" alive until done */]() {
        subscriber.on_completed();
      });
    });
}

/*! \brief Monitors an observable, logging a warning if it shows no activity (for the default-allotted time).
 * \tparam CreateSource A function-like object that accepts (a shared_ptr to) an ActivityMonitor and returns an rxcpp::observable<> whose activity is to be monitored.
 * \param io_context The I/O context to associate with the timer that monitors the observable.
 * \param description A description of the job being monitored.
 * \param createSource A function-like object that produces the source observable.
 * \return An observable emitting the source observable's items.
 *
 * \remark Allows .op(RxRecordActivity(monitor)) to be interspersed in the factory function's RX pipeline.
 */
template <typename CreateSource>
rxcpp::observable<typename decltype(std::declval<CreateSource>()(std::declval<std::shared_ptr<ActivityMonitor>>()))::value_type>
RxEnsureProgress(boost::asio::io_context& io_context, const std::string& description, const CreateSource& createSource) {
  return RxEnsureProgress(io_context, description, ActivityMonitor::DEFAULT_MAX_INACTIVE, createSource);
}

/// \brief Records that there's activity in an RX pipeline.
/// \code
///   myObs.op(RxRecordActivity(monitor, "barring the foo"))
/// \endcode
struct RxRecordActivity {
private:
  std::shared_ptr<ActivityMonitor> mMonitor;
  std::string mDescription;

public:
  explicit RxRecordActivity(std::shared_ptr<ActivityMonitor> monitor, const std::string& description)
    : mMonitor(monitor), mDescription(description) {
  }

  /// \param items The observable whose activity is to be recorded.
  /// \return An observable emitting the source observable's items.
  /// \tparam TItem The type of item produced by the observable.
  /// \tparam SourceOperator The source operator type included in the observable type.
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<TItem> operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    return items.tap(
      [monitor = mMonitor, description = mDescription](const TItem&) {monitor->activityOccurred("(busy) " + description); },
      [monitor = mMonitor, description = mDescription](std::exception_ptr ep) {monitor->activityOccurred("(failed) " + description + " (" + GetExceptionMessage(ep) + ")"); },
      [monitor = mMonitor, description = mDescription]() {monitor->activityOccurred("(done) " + description); }
    );
  }
};

}
