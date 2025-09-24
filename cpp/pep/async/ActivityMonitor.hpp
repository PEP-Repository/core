#pragma once

#include <pep/utils/Shared.hpp>

#include <mutex>
#include <optional>
#include <string>

#include <boost/core/noncopyable.hpp>
#include <boost/asio/steady_timer.hpp>

#include <pep/async/IoContext_fwd.hpp>

namespace pep {

class ActivityMonitor : public std::enable_shared_from_this<ActivityMonitor>, private boost::noncopyable {
private:
  const std::string mDescription;
  boost::asio::steady_timer mTimer;
  bool mTimerRunning = false;
  const decltype(mTimer)::duration mMaxInactive;
  std::mutex mMutex;
  std::optional<std::string> mLastActivityWhat;
  std::optional<decltype(mTimer)::time_point> mLastActivityWhen;

  ActivityMonitor(boost::asio::io_context& io_context, const std::string& jobDescription, decltype(mTimer)::duration maxInactive);
  void startTimer(decltype(mTimer)::duration alreadyElapsed = {});
  void handleTimerExpired();

public:
  static const std::chrono::minutes DEFAULT_MAX_INACTIVE; // 1 minute

  ~ActivityMonitor() noexcept;

  static std::shared_ptr<ActivityMonitor> Create(boost::asio::io_context& io_context, const std::string& jobDescription, decltype(mTimer)::duration maxInactive = DEFAULT_MAX_INACTIVE);

  void activityOccurred(const std::string& what);
};

}
