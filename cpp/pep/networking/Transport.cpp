#include <pep/networking/Transport.hpp>

namespace pep::networking {

Transport::Transport() {
  mLifeCycleStatusForwarding = LifeCycler::onStatusChange.subscribe([this](const StatusChange& change) {
    this->handleLifeCycleStatusChanged(change);
    });
}

Transport::~Transport() noexcept {
  mLifeCycleStatusForwarding.cancel();
}

Transport::ConnectivityStatus Transport::status() const noexcept {
  return static_cast<ConnectivityStatus>(LifeCycler::status());
}

Transport::ConnectivityStatus Transport::setConnectivityStatus(ConnectivityStatus status) {
  return static_cast<Transport::ConnectivityStatus>(this->setStatus(static_cast<Status>(status)));
}

void Transport::handleLifeCycleStatusChanged(const StatusChange& change) const {
  ConnectivityChange converted{ static_cast<ConnectivityStatus>(change.previous), static_cast<ConnectivityStatus>(change.updated) };
  onConnectivityChange.notify(converted);
}

}
