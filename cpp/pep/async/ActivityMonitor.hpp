#pragma once

#include <pep/utils/Shared.hpp>

#include <optional>
#include <string>

#include <boost/core/noncopyable.hpp>

#include <boost/asio.hpp>

namespace pep {

class ActivityMonitor : public std::enable_shared_from_this<ActivityMonitor>, private boost::noncopyable {
private:
  const std::string mDescription;
  boost::asio::deadline_timer mTimer;
  bool mTimerRunning = false;
  const boost::posix_time::milliseconds::tick_type mMsec;
  std::mutex mMutex;
  std::optional<std::string> mLastActivityWhat;
  std::optional<std::chrono::steady_clock::time_point> mLastActivityWhen;

  ActivityMonitor(boost::asio::io_service& io_service, const std::string& jobDescription, const std::chrono::milliseconds& maxInactive);
  void startTimer(boost::posix_time::milliseconds::tick_type msecAlreadyElapsed = 0);
  void handleTimerExpired();

public:
  static const std::chrono::milliseconds DEFAULT_MAX_INACTIVE; // 1 minute

  ~ActivityMonitor() noexcept;

  static std::shared_ptr<ActivityMonitor> Create(boost::asio::io_service& io_service, const std::string& jobDescription, const std::chrono::milliseconds& maxInactive = DEFAULT_MAX_INACTIVE);

  void activityOccurred(const std::string& what);
};

}
