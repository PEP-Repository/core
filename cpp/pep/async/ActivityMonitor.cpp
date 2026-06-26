#include <pep/async/ActivityMonitor.hpp>
#include <pep/utils/Log.hpp>

using namespace std::chrono_literals;

namespace pep {

const auto LogTag = "Activity monitor";

const std::chrono::seconds ActivityMonitor::DefaultMaxInactive = 1min;

ActivityMonitor::ActivityMonitor(boost::asio::io_context& io_context, const std::string& jobDescription, decltype(timer_)::duration maxInactive)
  : description_(jobDescription), timer_(io_context), maxInactive_(maxInactive) {
}

ActivityMonitor::~ActivityMonitor() noexcept {
  // No need to synchronize: our (last) owner is discarding us, so no one can invoke stuff on us anymore
  if (timerRunning_) {
    // TODO: prevent completion handlers from being invoked anyway.
    // See https://beta.boost.org/doc/libs/1_56_0/doc/html/boost_asio/reference/basic_waitable_timer/cancel/overload1.html :
    // "If the timer has already expired when cancel() is called [...] handlers can no longer be cancelled"
    timer_.cancel();
  }
}

// Callers must synchronize
void ActivityMonitor::startTimer(decltype(timer_)::duration alreadyElapsed) {
  assert(!timerRunning_);

  timer_.expires_after(maxInactive_ - alreadyElapsed);
  timer_.async_wait([weak = WeakFrom(*this)](const boost::system::error_code& ec) {
    if (ec != boost::asio::error::operation_aborted) {
      auto self = weak.lock();
      if (self == nullptr) {
        PEP_LOG(LogTag, Severity::Error) << "Inactivity detected for job that seems to have been completed";
      }
      else {
        self->handleTimerExpired();
      }
    }
  });

  timerRunning_ = true;
}

void ActivityMonitor::handleTimerExpired() {
  std::lock_guard<std::mutex> lock(mutex_);
  timerRunning_ = false;

  if (lastActivityWhen_.has_value()) {

    auto elapsed = decltype(lastActivityWhen_)::value_type::clock::now() - *lastActivityWhen_;
    assert(elapsed >= decltype(elapsed)::zero());
    lastActivityWhen_.reset();
    this->startTimer(elapsed);
  }
  else {
    PEP_LOG(LogTag, Severity::Warning) << "Inactivity detected for job: " << description_
      << ". Its last recorded activity was " << lastActivityWhat_.value_or("<none>");
  }
}

std::shared_ptr<ActivityMonitor> ActivityMonitor::Create(boost::asio::io_context& io_context, const std::string& jobDescription, decltype(timer_)::duration maxInactive) {
  auto result = std::shared_ptr<ActivityMonitor>(new ActivityMonitor(io_context, jobDescription, maxInactive));
  std::lock_guard<std::mutex> lock(result->mutex_);
  result->startTimer();
  return result;
}

void ActivityMonitor::activityOccurred(const std::string& what) {
  std::lock_guard<std::mutex> lock(mutex_);
  lastActivityWhat_ = what;
  if (timerRunning_) {
    lastActivityWhen_ = decltype(lastActivityWhen_)::value_type::clock::now();
  }
  else {
    PEP_LOG(LogTag, Severity::Info) << "Activity resumed for job: " << description_
      << " doing: " << what;
    this->startTimer();
  }
}

}
