#include <pep/async/ActivityMonitor.hpp>
#include <pep/utils/Log.hpp>

using namespace std::chrono_literals;

namespace pep {

const auto LOG_TAG = "Activity monitor";

const std::chrono::minutes ActivityMonitor::DEFAULT_MAX_INACTIVE = 1min;

ActivityMonitor::ActivityMonitor(boost::asio::io_context& io_context, const std::string& jobDescription, decltype(mTimer)::duration maxInactive)
  : mDescription(jobDescription), mTimer(io_context), mMaxInactive(maxInactive) {
}

ActivityMonitor::~ActivityMonitor() noexcept {
  // No need to synchronize: our (last) owner is discarding us, so no one can invoke stuff on us anymore
  if (mTimerRunning) {
    // TODO: prevent completion handlers from being invoked anyway.
    // See https://beta.boost.org/doc/libs/1_56_0/doc/html/boost_asio/reference/basic_waitable_timer/cancel/overload1.html :
    // "If the timer has already expired when cancel() is called [...] handlers can no longer be cancelled"
    mTimer.cancel();
  }
}

// Callers must synchronize
void ActivityMonitor::startTimer(decltype(mTimer)::duration alreadyElapsed) {
  assert(!mTimerRunning);

  mTimer.expires_after(mMaxInactive - alreadyElapsed);
  mTimer.async_wait([weak = WeakFrom(*this)](const boost::system::error_code& ec) {
    if (ec != boost::asio::error::operation_aborted) {
      auto self = weak.lock();
      if (self == nullptr) {
        LOG(LOG_TAG, error) << "Inactivity detected for job that seems to have been completed";
      }
      else {
        self->handleTimerExpired();
      }
    }
  });

  mTimerRunning = true;
}

void ActivityMonitor::handleTimerExpired() {
  std::lock_guard<std::mutex> lock(mMutex);
  mTimerRunning = false;

  if (mLastActivityWhen.has_value()) {

    auto elapsed = decltype(mLastActivityWhen)::value_type::clock::now() - *mLastActivityWhen;
    assert(elapsed >= decltype(elapsed)::zero());
    mLastActivityWhen.reset();
    this->startTimer(elapsed);
  }
  else {
    LOG(LOG_TAG, warning) << "Inactivity detected for job: " << mDescription
      << ". Its last recorded activity was " << mLastActivityWhat.value_or("<none>");
  }
}

std::shared_ptr<ActivityMonitor> ActivityMonitor::Create(boost::asio::io_context& io_context, const std::string& jobDescription, decltype(mTimer)::duration maxInactive) {
  auto result = std::shared_ptr<ActivityMonitor>(new ActivityMonitor(io_context, jobDescription, maxInactive));
  std::lock_guard<std::mutex> lock(result->mMutex);
  result->startTimer();
  return result;
}

void ActivityMonitor::activityOccurred(const std::string& what) {
  std::lock_guard<std::mutex> lock(mMutex);
  mLastActivityWhat = what;
  if (mTimerRunning) {
    mLastActivityWhen = decltype(mLastActivityWhen)::value_type::clock::now();
  }
  else {
    LOG(LOG_TAG, info) << "Activity resumed for job: " << mDescription
      << " doing: " << what;
    this->startTimer();
  }
}

}
