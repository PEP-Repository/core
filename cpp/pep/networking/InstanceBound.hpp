#pragma once

#include <stdexcept>
#include <type_traits>

namespace pep::networking {

/**
 * @brief Helper class for type safe downcasting of interrelated base types.
 * @tparam TInstance The type of (e.g. factory) instance to which each object will be bound.
 * 
 * @remark When base classes are interrelated, they must often be derived together, and derived types will need/want to access members
 *         of derived class instances. E.g.
 *
 *         class Networking {
 *           class Socket { ... };
 *           class Host {
 *             virtual Socket* createSocket() = 0;
 *             virtual void closeSocket(Socket* socket) = 0; // Overrides must accept base class Networking::Socket instances
 *           };
 *         };
 *
 *         class BoostBasedTlsNetworking : public Networking {
 *           class Socket : public Networking::Socket { ... };
 *           class Host : public Networking::Host {
 *             Networking::Socket* createSocket() override;
 *             void closeSocket(Networking::Socket* socket) override; // Receives base class parameter but needs (to access) derived class instance
 *           };
 *         };
 *
 * The BoostBasedTlsNetworking::Host::closeSocket method receives (a pointer to) a generic Networking::Socket and needs to downcast it to a
 * BoostBasedTlsNetworking::Socket to perform its actions. But it can't be sure that the downcast will be valid, since calling code may have passed
 * some other (non-Boost and/or non-TLS) socket as a parameter. The InstanceBound<> base class can be injected to perform run time validation on
 * such downcasts:
 *
 *         class Networking {
 *           class Socket : public InstanceBound<Networking> {
 *             Socket(Networking& networking) : InstanceBound<Networking>(networking) {}
 *           };
 *           class Host : public InstanceBound<Networking> {
 *             Host(Networking& networking) : InstanceBound<Networking>(networking) {}
 *             virtual Socket* createSocket() = 0;
 *             virtual void closeSocket(Socket* socket) = 0; // Overrides must accept base class Networking::Socket instances
 *           };
 *         };
 *
 *         class BoostBasedTlsNetworking : public Networking {
 *           class Socket : public Networking::Socket {
 *             Socket(BoostBasedTlsNetworking& boostTls) : Networking::Socket(boostTls) {}
 *           };
 *           class Host : public Networking::Host {
 *             Host(BoostBasedTlsNetworking& boostTls) : Networking::Host(boostTls) {}
 *             Networking::Socket* createSocket() override {
 *               return new Socket(this->boundInstance()); // Bind the socket to the host's instance
 *             }
 *             void closeSocket(Networking::Socket* socket) override {
 *               auto& derived = socket->downcastIfBoundTo<Socket>(this->boundInstance()); // Downcast the socket, ensuring it's bound to our own (BoostBasedTlsNetworking) instance
 *               // ... access BoostBasedTlsNetworking::Socket members ...
 *             }
 *           };
 *         };
 */
template <typename TInstance>
class InstanceBound {
private:
  const TInstance& mInstance;

  void verifyBoundTo(const TInstance& instance) const {
    if (&instance != &this->boundInstance()) {
      throw std::runtime_error("Object is bound to a different instance");
    }
  }

protected:
  /**
   * @brief Constructor.
   * @param instance The instance to which this object will be bound.
   */
  explicit InstanceBound(const TInstance& instance) noexcept
    : mInstance(instance) {
  }

  /**
   * @brief Downcasts this object to the specified type.
   * @param instance The instance to which this object must be bound.
   * @return This object, downcast to the specified type.
   * @remark Raises an exception if the object is bound to an instance other than the specified one.
   */
  template <typename TDerived>
  const TDerived& downcastIfBoundTo(const TInstance& instance) const {
    static_assert(std::is_base_of<InstanceBound, TDerived>::value, "Template parameter must inherit from InstanceBound");
    this->verifyBoundTo(instance);
    return static_cast<const TDerived&>(*this);
  }

  /**
   * @brief Downcasts this object to the specified type.
   * @param instance The instance to which this object must be bound.
   * @return This object, downcast to the specified type.
   * @remark Raises an exception if the object is bound to an instance other than the specified one.
   */
  template <typename TDerived>
  TDerived& downcastIfBoundTo(const TInstance& instance) {
    static_assert(std::is_base_of<InstanceBound, TDerived>::value, "Template parameter must inherit from InstanceBound");
    this->verifyBoundTo(instance);
    return static_cast<TDerived&>(*this);
  }

public:
  virtual ~InstanceBound() noexcept = default; // Instances of this class are likely to be used polymorphically (since they're intended to be downcast)

  /**
   * @brief Gets the instance to which this object is bound.
   * @return (A reference to) the instance to which this object is bound.
   */
  const TInstance& boundInstance() const noexcept { return mInstance; }
};

}
