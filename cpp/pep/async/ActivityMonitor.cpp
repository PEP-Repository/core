#include <pep/async/ActivityMonitor.hpp>
#include <pep/utils/Log.hpp>

namespace pep {

const auto LOG_TAG = "Activity monitor";

const std::chrono::milliseconds ActivityMonitor::DEFAULT_MAX_INACTIVE = std::chrono::minutes(1);

ActivityMonitor::ActivityMonitor(boost::asio::io_service& io_service, const std::string& jobDescription, const std::chrono::milliseconds& maxInactive)
  : mDescription(jobDescription), mTimer(io_service), mMsec(maxInactive.count()) {
}

ActivityMonitor::~ActivityMonitor() noexcept {
  // No need to synchronize: our (last) owner is discarding us, so no one can invoke stuff on us anymore
  if (mTimerRunning) {
    boost::system::error_code ec;
    mTimer.cancel(ec);
    if (ec) {
      // TODO: what happens when mTimer is destroyed? Is its handler still invoked?
      // Even if so, it won't be able to reacquire a shared_ptr<> to this instance
      LOG(LOG_TAG, error) << "Could not cancel timer for job: " << mDescription;
    }
  }
}

// Callers must synchronize
void ActivityMonitor::startTimer(boost::posix_time::milliseconds::tick_type msecAlreadyElapsed) {
  assert(!mTimerRunning);

  mTimer.expires_from_now(boost::posix_time::milliseconds(mMsec - msecAlreadyElapsed));
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
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - *mLastActivityWhen).count();
    assert(elapsed >= 0);
    mLastActivityWhen.reset();
    this->startTimer(elapsed);
  }
  else {
    LOG(LOG_TAG, warning) << "Inactivity detected for job: " << mDescription
      << ". Its last recorded activity was " << mLastActivityWhat.value_or("<none>");
  }
}

std::shared_ptr<ActivityMonitor> ActivityMonitor::Create(boost::asio::io_service& io_service, const std::string& jobDescription, const std::chrono::milliseconds& maxInactive) {
  auto result = std::shared_ptr<ActivityMonitor>(new ActivityMonitor(io_service, jobDescription, maxInactive));
  std::lock_guard<std::mutex> lock(result->mMutex);
  result->startTimer();
  return result;
}

void ActivityMonitor::activityOccurred(const std::string& what) {
  std::lock_guard<std::mutex> lock(mMutex);
  mLastActivityWhat = what;
  if (mTimerRunning) {
    mLastActivityWhen = std::chrono::steady_clock::now();
  }
  else {
    LOG(LOG_TAG, info) << "Activity resumed for job: " << mDescription
      << " doing: " << what;
    this->startTimer();
  }
}

}
