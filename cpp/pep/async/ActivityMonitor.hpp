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
  const std::string description_;
  boost::asio::steady_timer timer_;
  bool timerRunning_ = false;
  const decltype(timer_)::duration maxInactive_;
  std::mutex mutex_;
  std::optional<std::string> lastActivityWhat_;
  std::optional<decltype(timer_)::time_point> lastActivityWhen_;

  ActivityMonitor(boost::asio::io_context& io_context, const std::string& jobDescription, decltype(timer_)::duration maxInactive);
  void startTimer(decltype(timer_)::duration alreadyElapsed = {});
  void handleTimerExpired();

public:
  static const std::chrono::seconds DEFAULT_MAX_INACTIVE; // 1 minute

  ~ActivityMonitor() noexcept;

  static std::shared_ptr<ActivityMonitor> Create(boost::asio::io_context& io_context, const std::string& jobDescription, decltype(timer_)::duration maxInactive = DEFAULT_MAX_INACTIVE);

  void activityOccurred(const std::string& what);
};

}
