#include <pep/async/RxRequireCount.hpp>
#include <pep/key-components/KeyComponentSerializers.hpp>
#include <pep/key-components/KeyComponentServerProxy.hpp>

namespace pep {

rxcpp::observable<KeyComponentResponse> KeyComponentServerProxy::requestKeyComponent(SignedKeyComponentRequest request) const {
  return this->sendRequest<KeyComponentResponse>(std::move(request))
    .op(RxGetOne());
}


}
