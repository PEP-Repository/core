#pragma once

#include <memory>
#include <stdexcept>

namespace pep {
/*!
 * \brief Mixin to help object creation as a shared_ptr. Especially helpful for classes that (directly or indirectly) inherit std::enable_shared_from_this<>.
 *
 * For a class `Foo`, inherit SharedConstructor<Foo> to have it provide factory method `Foo::Create(<normal constructor arguments>)` .
 * To enforce instantiation using the factory method, also make Foo's constructor(s) private and be-friend class SharedConstructor<Foo>.
 */
template <typename T>
class SharedConstructor {
  friend T;
  SharedConstructor() = default;
public:
  template <typename... Args>
  static std::shared_ptr<T> Create(Args&&... args) {
    return std::shared_ptr<T>(new T(std::forward<Args>(args)...)); //Cannot use make_shared with non-public constructor (which is exactly what we want to achieve). Bit less efficient, but still safe
  }
};

/*
 * \brief For instances of types that inherit std::enable_shared_from_this<>, returns a strongly-typed shared_ptr to that instance.
 * \param instance The instance for which to retrieve a shared_ptr.
*/
template <typename T>
std::shared_ptr<T> SharedFrom(T& instance){
  return std::static_pointer_cast<T>(instance.shared_from_this());
}

/*
 * \brief For instances of types that inherit std::enable_shared_from_this<>, returns a strongly-typed weak_ptr to that instance.
 * \param instance The instance for which to retrieve a weak_ptr.
*/
template <typename T>
std::weak_ptr<T> WeakFrom(T& instance) {
  return std::static_pointer_cast<T>(instance.weak_from_this().lock());
}

/*
 * \brief Converts a weak_ptr<> to a shared_ptr<> to the same instance, raising an exception if the instance has been discarded.
 * \param weak The weak_ptr<> to the instance.
 * \return A shared_ptr<> to the instance.
*/
template <typename T>
std::shared_ptr<T> AcquireShared(std::weak_ptr<T> weak) {
  auto result = weak.lock();
  if (result == nullptr) {
    throw std::runtime_error("Can't acquire shared pointer from weak pointer to discarded instance");
  }
  return result;
}

/*
 * \brief Creates a heap-allocated copy of the specified instance.
 * \param instance The instance for which to create a copy.
 * \return A shared_ptr to the created copy.
 * \remark Saves the caller the trouble of (looking up and) typing the instance's full type name.
*/
template <typename T>
auto MakeSharedCopy(T&& instance) {
  using NonRef = typename std::remove_reference<T>::type;
  using Plain = typename std::remove_cv<NonRef>::type;
  return std::make_shared<Plain>(std::forward<T>(instance));
}

template <typename T>
std::shared_ptr<const T> PtrAsConst(std::shared_ptr<T> ptr) {
  return std::shared_ptr<const T>(std::move(ptr));
}

}
