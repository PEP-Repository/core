#pragma once

#include <pep/utils/OperationResult.hpp>
#include <pep/utils/LifeCycler.hpp>
#include <pep/utils/TypeTraits.hpp>
#include <boost/core/noncopyable.hpp>

namespace pep::networking {

using SizedTransfer = OperationInvocation<size_t>;
using DelimitedTransfer = OperationInvocation<std::string>;

/*!
 * \brief Interface for classes that communicate binary data asynchronously (across a network).
 * \remark Inheritors must cancel pending read and/or write actions when close() is called.
 *         This implies that derived classes themselves alive (e.g. using shared_from_this) for
 *         long enough to coordinate calls to the asyncRead[Until], asyncWrite, and close methods,
 *         and invocations of the TransferHandler callbacks.
 * \remark Note the _private_ inheritance from LifeCycler: we want the functionality, but
 *         expose it to consuming code in terms of "connecting" and "disconnecting" instead of
 *         "initializing" and "finalizing".
 */
class Transport: private LifeCycler {
public:
  ~Transport() noexcept override;

  enum class ConnectivityStatus {
    unconnected   = ToUnderlying(LifeCycler::Status::uninitialized),
    reconnecting  = ToUnderlying(LifeCycler::Status::reinitializing),
    connecting    = ToUnderlying(LifeCycler::Status::initializing),
    connected     = ToUnderlying(LifeCycler::Status::initialized),
    disconnecting = ToUnderlying(LifeCycler::Status::finalizing),
    disconnected  = ToUnderlying(LifeCycler::Status::finalized)
  };

  ConnectivityStatus status() const noexcept;

  struct ConnectivityChange {
    ConnectivityStatus previous;
    ConnectivityStatus updated;
  };

  const Event<Transport, ConnectivityChange> onConnectivityChange;

  /*
   * \brief Indicates whether the transport is currently open for business, i.e. fully connected to its counterpart
   *        (on the other side of the network)
   * \remark The return value of this method is not (necessarily) equal to !isClosed(). E.g. both isConnected() and
   *         isClosed() will return false while connectivity is still being established, or after an underlying layer
   *         has closed itself due to an unrecoverable error.
   */
  bool isConnected() const noexcept { return this->status() == ConnectivityStatus::connected; }

  /*
   * \brief Indicates whether the transport has been closed, i.e. fully shut down without a chance of being reconnected.
   */
  bool isClosed() const noexcept { return this->status() == ConnectivityStatus::disconnected; }

  /*
   * \brief Returns (a string representation of) the address of the connected party.
   * \remark May only be invoked when this->isConnected() .
   */
  virtual std::string remoteAddress() const = 0;

  /*
   * \brief Closes the transport.
   * \remark Note that the transport may reconnect, so invoking this method won't necessarily make isClosed become true.
   */
  virtual void close() = 0;

  /*
   * \brief Asynchronously reads a specified amount of data from the Transport
   * \param destination The memory into which the received data will be stored. Caller must provide sufficient
   *                    capacity to store the requested number of bytes, and must ensure that the memory (region)
   *                    remains valid until the callback function is invoked.
   * \param bytes The number of bytes to read
   * \param onTransferred A function that will be invoked when the read action has completed or failed
   */
  virtual void asyncRead(void* destination, size_t bytes, const SizedTransfer::Handler& onTransferred) = 0;

  /*
   * \brief Asynchronously reads data from the Transport until specified data is received
   * \param delimiter The bytes marking the end of the data to be read. Caller must ensure that the memory area remains
   *                  valid until the callback function is invoked. If the read completes successfully, the delimiter
   *                  bytes will be included at the end of the data passed to the callback function.
   * \param onTransferred A function that will be invoked when the read action has completed or failed
   */
  virtual void asyncReadUntil(const char* delimiter, const DelimitedTransfer::Handler& onTransferred) = 0;

  /*
   * \brief Asynchronously reads all data from the Transport (until the remote party disconnects)
   * \param onTransferred A function that will be invoked when the read action has completed or failed
   * \remark Attempting to schedule a new (read or write) transfer from the "onTransferred" callback will produce
   *         an exception, since this Transport will be scheduled to be closed. To perform followup reads or writes,
   *         either use a new Transport, or (if applicable) wait for this one to reconnect.
   */
  virtual void asyncReadAll(const DelimitedTransfer::Handler& onTransferred) = 0;

  /*
   * \brief Asynchronously writes a specified amount of data to the Transport
   * \param destination The memory containing the data to be written. Caller must provide at least the specified number
   *                    of bytes, and must ensure that the memory (region) remains valid until the callback function is
   *                    invoked.
   * \param bytes The number of bytes to write
   * \param onTransferred A function that will be invoked when the write action has completed or failed
   */
  virtual void asyncWrite(const void* source, size_t bytes, const SizedTransfer::Handler& onTransferred) = 0;

protected:
  Transport();

  ConnectivityStatus setConnectivityStatus(ConnectivityStatus status);

private:
  EventSubscription mLifeCycleStatusForwarding;

  void handleLifeCycleStatusChanged(const StatusChange& change) const;
};

}
