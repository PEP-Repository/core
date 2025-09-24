#pragma once

#include <pep/utils/OperationResult.hpp>
#include <memory>

namespace pep::networking {

/*!
 * \brief Helper class for connection attempt notification.
 * \tparam T The type of connection that was attempted.
 * \remark Provides nested types for connection attempt notifications and the handling thereof.
 */
template <typename T>
using ConnectivityAttempt = OperationInvocation<std::shared_ptr<T>>;

}
