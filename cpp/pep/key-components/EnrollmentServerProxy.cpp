#include <pep/async/RxRequireCount.hpp>
#include <pep/key-components/EnrollmentServerProxy.hpp>
#include <pep/key-components/KeyComponentSerializers.hpp>

namespace pep {

rxcpp::observable<KeyComponentResponse> EnrollmentServerProxy::requestKeyComponent(SignedKeyComponentRequest request) const {
  return this->sendRequest<KeyComponentResponse>(std::move(request))
    .op(RxGetOne());
}


}
