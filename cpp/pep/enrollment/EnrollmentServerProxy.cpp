#include <pep/async/RxRequireCount.hpp>
#include <pep/enrollment/EnrollmentServerProxy.hpp>
#include <pep/enrollment/KeyComponentSerializers.hpp>

namespace pep {

rxcpp::observable<KeyComponentResponse> EnrollmentServerProxy::requestKeyComponent(SignedKeyComponentRequest request) const {
  return this->sendRequest<KeyComponentResponse>(std::move(request))
    .op(RxGetOne());
}


}
