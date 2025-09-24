#pragma once

#include <boost/noncopyable.hpp>
#include <utility>

namespace pep {

/*!
 * \brief Debug helper: encapsulate a `const InstanceTracker<MyType>` in your class to assign a unique ID to every instance.
 *        The ID can then be e.g. included in logging or inspected by a debugger.
 */
template <typename T>
class InstanceTracker : boost::noncopyable {
private:
  static size_t count;

public:
  const size_t id;
  const size_t incarnation = 0U;

  InstanceTracker() noexcept : id(count++) {} // New instance gets a new ID
  InstanceTracker(InstanceTracker&& other) noexcept : id(other.id), incarnation(other.incarnation + 1U) {} // Move-construction is a new incarnation of the same instance

  InstanceTracker& operator=(InstanceTracker&&) = delete; // Don't reassign a different ID to an existing instance
};

template <typename T>
size_t InstanceTracker<T>::count = 0U;

}
