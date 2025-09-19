#pragma once

#include <pep/utils/Event.hpp>

namespace pep {

/*!
 * \brief (Base) class that progresses through a life cycle and provides status change notifications.
 */
class LifeCycler : boost::noncopyable {
public:
  /// \brief Supported status(valu)es
  enum class Status {
    /// \brief The instance (has been created but) requires further initialization
    uninitialized,
    /// \brief Initialization is being _re_started
    reinitializing,
    /// \brief The instance is being initialized
    initializing,
    /// \brief The instance (has been initialized and) is fully usable
    initialized,
    /// \brief The instance is shutting down without a chance of reinitialization
    finalizing,
    /// \brief Shutdown is complete: the instance won't become usable again
    finalized
  };

  /// \brief Parameter for the onStatusChange event
  struct StatusChange {
    Status previous;
    Status updated;
  };

private:
  Status mStatus = Status::uninitialized;

protected:
  LifeCycler() noexcept = default;

  /* !
   * \brief Assigns the specified status to this instance.
   * \param status: The status to assign.
   * \return The previous status.
   * \remark Setting an initialized instance to "initializing" will emit two notifications: (1) from "initialized" to "reinitializing" and (2) from "reinitializing" to "initializing".
   */
  Status setStatus(Status status);

public:
  /// \brief Destructor
  virtual ~LifeCycler() noexcept;

  /// \brief Event that is notified when the instance's life cycle status changes from one value to another
  /// \remark The event will only be notified for actual changes: multiple calls to "setStatus" for the same value won't be notified
  const Event<LifeCycler, StatusChange> onStatusChange;

  /// \brief Produces the instance's current life cycle status
  /// \return The instance's current life cycle status
  Status status() const noexcept { return mStatus; }
};

}
