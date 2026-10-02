#pragma once

#include <emscripten/val.h>

#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-observe_on.hpp>

#include <cstddef>
#include <memory>
#include <string>

namespace pep::weblib {

rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>>
ReadableStreamToMessageBatches(emscripten::val reader, std::size_t pageSize, rxcpp::observe_on_one_worker ioWorker);

}
