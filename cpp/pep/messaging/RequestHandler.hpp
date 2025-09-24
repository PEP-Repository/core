#pragma once

#include <pep/serialization/Serialization.hpp>
#include <pep/messaging/MessageSequence.hpp>
#include <cassert>

namespace pep::messaging {

/**
 * @brief Base class that allows derived classes to register methods that handle a specific request type.
 *        The "handleRequest" method can then dispatch serialized requests to the appropriate registered method.
 */
class RequestHandler {
public:
  /**
   * @brief Handles a serialized request, producing a sequence-of-sequence-of serialized responses.
   * @param magic the message magic indicating the request's type. An exception is raised if no method has been registered that handles this type.
   * @param message the serialized request.
   * @param tail followup messages associated with the primary request. Pass an empty MessageSequence if the request has no followup messages.
   * @return a sequence of sequence of serialized response messages.
   */
  MessageBatches handleRequest(MessageMagic magic, std::shared_ptr<std::string> message, MessageSequence tail);

protected:
  /**
   * @brief Registers one or more member functions as request handlers.
   * @param instance Caller must pass `this` here, so that ThisT resolves to the specific (derived) type of the caller.
   * @param methods pointers to the member functions of ThisT that will be registered as handlers.
   * @tparam ThisT The (derived) type of the RequestHandler instance on which the methods are registered.
   * @remark All MethodPtrTs must be pointers to methods with one of the following signatures:
   *         - MessageBatches (ThisT::*)(std::shared_ptr<SomeRequestType>)
   *         - MessageBatches (ThisT::*)(std::shared_ptr<SomeRequestType>, MessageSequence)
   * @remark Overwrites any previously registered handler(s) for the same request type(s).
   */
  template <typename ThisT, typename... MethodPtrTs>
  static void RegisterRequestHandlers(ThisT& instance, MethodPtrTs... methods) {
    static_assert(std::is_base_of_v<RequestHandler, ThisT>, "Template parameter type must inherit from this class");
    static_assert(sizeof...(methods) >= 1);
    (RegisterSingleRequestHandler(instance, methods), ...);
  }

private:
  class Method {
  public:
    virtual MessageBatches handle(RequestHandler& instance, std::shared_ptr<std::string> message, MessageSequence tail) const = 0;
    virtual ~Method() = default;
  };

  template <typename DeclaringT, typename RequestT>
  class TypedMethod : public Method {
  public:
    using DeclaringClass = DeclaringT;
    using Request = RequestT;

  protected:
    virtual MessageBatches handleRequest(DeclaringT& instance, std::shared_ptr<RequestT> request, MessageSequence tail) const = 0;

  public:
    MessageBatches handle(RequestHandler& instance, std::shared_ptr<std::string> message, MessageSequence tail) const override {
      // This downcast is valid because it's only performed through one of the "mMethods" of the "instance".
      // Therefore RegisterMethod must have been invoked, which guarantees that the "instance" is a "DeclaringT".
      DeclaringT& downcast = static_cast<DeclaringT&>(instance);

      auto request = std::make_shared<RequestT>(Serialization::FromString<RequestT>(*message, false));
      return this->handleRequest(downcast, request, tail);
    }
  };

  template <typename DeclaringT, typename RequestT>
  class UnaryMethod : public TypedMethod<DeclaringT, RequestT> {
  public:
    using Pointer = MessageBatches(DeclaringT::*)(std::shared_ptr<RequestT>);

  private:
    Pointer mPointer;

  protected:
    MessageBatches handleRequest(DeclaringT& instance, std::shared_ptr<RequestT> request, MessageSequence tail) const override {
      // TODO: ensure that "tail" is empty
      return (instance.*mPointer)(request);
    }

  public:
    explicit UnaryMethod(Pointer pointer) : mPointer(pointer) { assert(mPointer != nullptr); }
  };

  template <typename DeclaringT, typename RequestT>
  class BinaryMethod : public TypedMethod<DeclaringT, RequestT> {
  public:
    using Pointer = MessageBatches(DeclaringT::*)(std::shared_ptr<RequestT>, MessageSequence);

  private:
    Pointer mPointer;

  protected:
    MessageBatches handleRequest(DeclaringT& instance, std::shared_ptr<RequestT> request, MessageSequence tail) const override {
      return (instance.*mPointer)(request, tail);
    }

  public:
    explicit BinaryMethod(Pointer pointer) : mPointer(pointer) { assert(mPointer != nullptr); }
  };

  template <typename MethodT>
  static void RegisterMethod(typename MethodT::DeclaringClass& instance, std::shared_ptr<MethodT> method) {
    RequestHandler& upcast = instance;
    upcast.mMethods.insert_or_assign(MessageMagician<typename MethodT::Request>::GetMagic(), method);
  }

  template <typename DeclaringT, typename RequestT>
  static void RegisterSingleRequestHandler(
    DeclaringT& instance, MessageBatches(DeclaringT::* method)(std::shared_ptr<RequestT>)) {
    RegisterMethod(instance, std::make_shared<UnaryMethod<DeclaringT, RequestT>>(method));
  }

  template <typename DeclaringT, typename RequestT>
  static void RegisterSingleRequestHandler(
    DeclaringT& instance,
    MessageBatches(DeclaringT::* method)(std::shared_ptr<RequestT>, MessageSequence)) {
    RegisterMethod(instance, std::make_shared<BinaryMethod<DeclaringT, RequestT>>(method));
  }

  std::unordered_map<MessageMagic, std::shared_ptr<Method>> mMethods;
};

}
