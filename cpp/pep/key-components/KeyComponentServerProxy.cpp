#include <pep/async/RxRequireCount.hpp>
#include <pep/key-components/KeyComponentSerializers.hpp>
#include <pep/key-components/KeyComponentServerProxy.hpp>
#include <pep/key-components/KeyComponentMessages.hpp> // for KeyComponentResponse, SignedKeyComponentRequest

#include <rxcpp/rx-lite.hpp>

#include <utility>

namespace pep {

rxcpp::observable<KeyComponentResponse> KeyComponentServerProxy::requestKeyComponent(SignedKeyComponentRequest request) const {
  return this->sendRequest<KeyComponentResponse>(std::move(request))
    .op(RxGetOne());
}


}
