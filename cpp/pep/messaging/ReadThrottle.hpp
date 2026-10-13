#pragma once

namespace pep::messaging {

/// \brief Lets a request handler temporarily stop the \c Connection that delivered a request from reading further messages from its socket.
/// \details While paused, no new messages are read, so the operating system's receive buffer fills up and TCP flow control makes the peer slow down.
///   This is how a handler that consumes a request's tail more slowly than it arrives (e.g. because it is waiting for a slow backend) can prevent
///   an unbounded amount of data from piling up in memory.
/// \remark Pausing is per \c Connection and therefore also delays every other message (of every other stream) on that connection.
/// \remark A message that has already (partly) been received when \c pause is invoked is still delivered, so a handler may receive up to one more message.
/// \remark Not thread-safe: use from the connection's \c io_context thread only.
/// \remark Instances automatically resume when they're destroyed, and become inert when their connection is closed or reconnects.
class ReadThrottle {
public:
  virtual ~ReadThrottle() = default;

  /// \brief Stops the connection from reading further messages. Has no effect if this instance is already paused.
  virtual void pause() = 0;
  /// \brief Allows the connection to read messages again, unless another paused instance still prevents it. Has no effect if this instance isn't paused.
  virtual void resume() = 0;
};

}
