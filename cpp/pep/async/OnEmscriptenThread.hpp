#pragma once

#include <rxcpp/operators/rx-observe_on.hpp>
#include <sys/types.h>

namespace pep {

rxcpp::observe_on_one_worker observe_on_emscripten(::pthread_t thread);
rxcpp::observe_on_one_worker observe_on_emscripten_main_thread();

}
