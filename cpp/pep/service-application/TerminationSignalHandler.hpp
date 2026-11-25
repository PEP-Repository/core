#pragma once

#include <pep/async/IoContextThread.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/core/noncopyable.hpp>

#include <functional>
#include <memory>
#include <optional>

namespace pep {

/// \brief Invokes a callback when the process receives SIGINT or SIGTERM, so that it can exit normally instead of being killed.
/// \remark Only the first signal is handled: a second one kills the process.
/// \remark Other threads keep running until the callback takes effect: don't use this to cancel work in clients (see core#2008).
class TerminationSignalHandler : boost::noncopyable {
private:
  std::shared_ptr<boost::asio::io_context> ioContext_;
  boost::asio::signal_set signals_;
  std::optional<IoContextThread> thread_; // Declared last: must be joined before the other fields are destroyed

public:
  /// \brief Constructor.
  /// \param onTermination Invoked on a separate thread when the process is asked to terminate.
  explicit TerminationSignalHandler(std::function<void()> onTermination);
};

}
